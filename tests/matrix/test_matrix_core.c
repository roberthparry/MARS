#include "test_matrix.h"
#include "matrix/matrix_internal.h"

/*
 * The public API now exposes DV_ZERO / DV_ONE as read-only immortal nodes.
 * Matrix fixture builders in this file still pass borrowed symbolic constants
 * through mutable dval_t * slots, so cast locally in tests rather than
 * weakening the API constness.
 */
#define DV_ZERO ((dval_t *)DV_ZERO)
#define DV_ONE  ((dval_t *)DV_ONE)

static void check_mat_num_local(const char *label,
                                matrix_t *got,
                                matrix_t *expected,
                                double tol)
{
    int ok = 1;

    if (!got || !expected ||
        mat_get_row_count(got) != mat_get_row_count(expected) ||
        mat_get_col_count(got) != mat_get_col_count(expected)) {
        check_bool(label, false);
        return;
    }

    for (size_t i = 0; i < mat_get_row_count(got) && ok; ++i) {
        for (size_t j = 0; j < mat_get_col_count(got) && ok; ++j) {
            number_t g = mat_get_num(got, i, j);
            number_t e = mat_get_num(expected, i, j);
            number_t diff = num_sub(g, e);
            number_t mag = num_abs(diff);
            double err = num_to_double(mag);

            if (!(err < tol))
                ok = 0;

            num_destroy(&mag);
            num_destroy(&diff);
            num_destroy(&e);
            num_destroy(&g);
        }
    }

    check_bool(label, ok);
}

static void test_creation(void)
{
    printf(C_CYAN "TEST: creation of all matrix types\n" C_RESET);

    matrix_t *Ad = test_mat_dense_d(2, 3);
    matrix_t *Aqf = test_mat_dense_qf(3, 4);
    matrix_t *Aqc = test_mat_dense_qc(4, 5);

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
        matrix_t *A = test_mat_create_d(2, 2, vals);
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
        matrix_t *B = test_mat_create_qf(2, 2, vals);
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

        matrix_t *C = test_mat_create_qc(2, 2, vals);
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
        matrix_t *A = test_mat_dense_d(2, 2);
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
        matrix_t *B = test_mat_dense_qf(2, 2);
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
        matrix_t *C = test_mat_dense_qc(2, 2);
        qcomplex_t z = qc_make(qf_from_double(1.0), qf_from_double(-3.0));
        mat_set(C, 1, 1, &z);

        print_mqc("C after write", C);

        qcomplex_t vals[4];
        mat_get_data(C, vals);
        check_qc_val("write qcomplex C[1,1]", vals[3], z, 1e-30);

        mat_free(C);
    }
}

static void test_number_creation_and_readback(void)
{
    printf(C_CYAN "TEST: number_t matrix creation and owned readback\n" C_RESET);

    number_t vals[4];
    number_t got;
    number_t flat[4];
    matrix_t *A;

    vals[0] = num_create_from_string("1/2");
    vals[1] = num_create_from_long(3);
    vals[2] = num_create_from_string("1 + 2i");
    vals[3] = num_create_from_string("1.25");
    num_set_prec_bits(&vals[3], 512);

    A = mat_create_num(2, 2, vals);
    check_bool("mat_create_num non-null", A != NULL);
    check_bool("mat_create_num -> MAT_TYPE_NUMBER",
               A != NULL && mat_typeof(A) == MAT_TYPE_NUMBER);

    got = mat_get_num(A, 0, 0);
    check_bool("mat_get_num exact rational", num_eq(got, vals[0]));
    num_destroy(&got);

    got = mat_get_num(A, 1, 0);
    check_bool("mat_get_num complex value", num_eq(got, vals[2]));
    check_bool("mat_get_num complex stays non-real", !num_is_real(got));
    num_destroy(&got);

    got = mat_get_num(A, 1, 1);
    check_bool("mat_get_num preserves multiprecision value", num_eq(got, vals[3]));
    check_bool("mat_get_num preserves precision bits", num_get_prec_bits(got) == 512);
    num_destroy(&got);

    mat_get_data_num(A, flat);
    check_bool("mat_get_data_num[0]", num_eq(flat[0], vals[0]));
    check_bool("mat_get_data_num[1]", num_eq(flat[1], vals[1]));
    check_bool("mat_get_data_num[2]", num_eq(flat[2], vals[2]));
    check_bool("mat_get_data_num[3] precision", num_get_prec_bits(flat[3]) == 512);

    for (size_t i = 0; i < 4; ++i) {
        num_destroy(&flat[i]);
        num_destroy(&vals[i]);
    }
    mat_free(A);
}

static void test_number_special_constructors(void)
{
    printf(C_CYAN "TEST: number_t matrix specialised constructors\n" C_RESET);

    number_t diag[2];
    number_t got;
    matrix_t *I;
    matrix_t *D;

    diag[0] = num_create_from_string("2/3");
    diag[1] = num_create_from_string("5");

    I = mat_create_identity_num(2);
    D = mat_create_diagonal_num(2, diag);

    check_bool("mat_create_identity_num non-null", I != NULL);
    check_bool("mat_create_diagonal_num non-null", D != NULL);

    got = mat_get_num(I, 0, 0);
    check_bool("identity number diag one", num_eq(got, NUM_ONE));
    num_destroy(&got);

    got = mat_get_num(I, 0, 1);
    check_bool("identity number offdiag zero", num_eq(got, NUM_ZERO));
    num_destroy(&got);

    got = mat_get_num(D, 0, 0);
    check_bool("diagonal number entry 0", num_eq(got, diag[0]));
    num_destroy(&got);

    got = mat_get_num(D, 1, 1);
    check_bool("diagonal number entry 1", num_eq(got, diag[1]));
    num_destroy(&got);

    got = mat_get_num(D, 0, 1);
    check_bool("diagonal number offdiag zero", num_eq(got, NUM_ZERO));
    num_destroy(&got);

    num_destroy(&diag[0]);
    num_destroy(&diag[1]);
    mat_free(I);
    mat_free(D);
}

static void test_number_matrix_arithmetic(void)
{
    printf(C_CYAN "TEST: number_t matrix arithmetic and promotion\n" C_RESET);

    number_t avals[4];
    number_t bvals[4];
    number_t scalar;
    number_t got;
    qfloat_t qfvals[4];
    matrix_t *A;
    matrix_t *B;
    matrix_t *C;
    matrix_t *Q;
    matrix_t *M;
    number_t expected;

    avals[0] = num_create_from_long(1);
    avals[1] = num_create_from_long(2);
    avals[2] = num_create_from_long(3);
    avals[3] = num_create_from_long(4);
    bvals[0] = num_create_from_long(5);
    bvals[1] = num_create_from_long(6);
    bvals[2] = num_create_from_long(7);
    bvals[3] = num_create_from_long(8);

    A = mat_create_num(2, 2, avals);
    B = mat_create_num(2, 2, bvals);
    C = mat_add(A, B);

    check_bool("number + number -> MAT_TYPE_NUMBER",
               C != NULL && mat_typeof(C) == MAT_TYPE_NUMBER);

    got = mat_get_num(C, 0, 0);
    expected = num_create_from_long(6);
    check_bool("number add [0,0] = 6", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);
    got = mat_get_num(C, 1, 1);
    expected = num_create_from_long(12);
    check_bool("number add [1,1] = 12", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);
    mat_free(C);

    C = mat_mul(A, B);
    check_bool("number * number -> MAT_TYPE_NUMBER",
               C != NULL && mat_typeof(C) == MAT_TYPE_NUMBER);
    got = mat_get_num(C, 0, 0);
    expected = num_create_from_long(19);
    check_bool("number mul [0,0] = 19", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);
    got = mat_get_num(C, 1, 1);
    expected = num_create_from_long(50);
    check_bool("number mul [1,1] = 50", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);
    mat_free(C);

    scalar = num_create_from_string("1/2");
    C = mat_scalar_mul_num(A, &scalar);
    check_bool("mat_scalar_mul_num non-null", C != NULL);
    got = mat_get_num(C, 0, 1);
    check_bool("scalar mul number exact half", num_eq(got, NUM_ONE));
    num_destroy(&got);
    mat_free(C);

    C = mat_scalar_div_num(B, &scalar);
    check_bool("mat_scalar_div_num non-null", C != NULL);
    got = mat_get_num(C, 0, 0);
    expected = num_create_from_long(10);
    check_bool("scalar div number by half", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);
    mat_free(C);
    num_destroy(&scalar);

    qfvals[0] = qf_from_double(0.5);
    qfvals[1] = qf_from_double(1.5);
    qfvals[2] = qf_from_double(2.5);
    qfvals[3] = qf_from_double(3.5);
    Q = test_mat_create_qf(2, 2, qfvals);
    M = mat_add(A, Q);
    check_bool("number + qfloat promotes to number",
               M != NULL && mat_typeof(M) == MAT_TYPE_NUMBER);
    got = mat_get_num(M, 0, 0);
    expected = num_create_from_string("1.5");
    check_bool("number + qfloat value", num_eq(got, expected));
    num_destroy(&expected);
    num_destroy(&got);

    mat_free(M);
    mat_free(Q);
    mat_free(B);
    mat_free(A);
    for (size_t i = 0; i < 4; ++i) {
        num_destroy(&avals[i]);
        num_destroy(&bvals[i]);
    }
}

static char *format_matrix_core_num_at_own_precision(const number_t value)
{
    char fmt[32];
    char *out;
    size_t significant_digits = num_get_prec_digits(value);
    int needed;
    size_t precision;

    if (num_is_exact(value) || significant_digits == 0u)
        return num_to_string(value);

    precision = significant_digits > 0u ? significant_digits - 1u : 0u;
    snprintf(fmt, sizeof(fmt), "%%.%zuN", precision);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, fmt, value) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

static char *format_matrix_core_num_error(const number_t value)
{
    int needed = num_sprintf(NULL, 0u, "%.6N", value);
    char *out;

    if (needed < 0)
        return num_to_string(value);
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, "%.6N", value) < 0) {
        free(out);
        return num_to_string(value);
    }
    return out;
}

static number_t matrix_core_num_error_magnitude(const number_t got,
                                                const number_t expected)
{
    number_t promoted_got = num_clone(got);
    number_t diff;
    number_t error;

    if (num_get_prec_bits(expected) > 0u)
        num_set_prec_bits(&promoted_got, num_get_prec_bits(expected));
    diff = num_sub(promoted_got, expected);
    num_destroy(&promoted_got);

    if (num_is_real(diff)) {
        error = num_abs(diff);
        num_destroy(&diff);
        return error;
    }

    {
        number_t real = num_real_part(diff);
        number_t imag = num_imag_part(diff);
        number_t mag = num_hypot(real, imag);

        num_destroy(&imag);
        num_destroy(&real);
        num_destroy(&diff);
        return mag;
    }
}

static void print_matrix_core_num_comparison(const char *label,
                                             const number_t got,
                                             const number_t expected)
{
    char *expected_text = format_matrix_core_num_at_own_precision(expected);
    char *got_text = format_matrix_core_num_at_own_precision(got);
    number_t error = matrix_core_num_error_magnitude(got, expected);
    char *error_text = format_matrix_core_num_error(error);

    printf("    %s\n", label);
    printf("        expected = %s\n", expected_text ? expected_text : "(unavailable)");
    printf("        got      = %s\n", got_text ? got_text : "(unavailable)");
    printf("        error    = %s\n", error_text ? error_text : "(unavailable)");
    printf("        precision: %zu bits, %zu significant digits\n",
           num_get_prec_bits(got), num_get_prec_digits(got));

    free(error_text);
    num_destroy(&error);
    free(got_text);
    free(expected_text);
}

static void test_number_det_and_inverse(void)
{
    printf(C_CYAN "TEST: determinant/inverse (number_t)\n" C_RESET);

    {
        number_t diag[2];
        number_t det;
        number_t got;
        number_t expected;
        matrix_t *A;
        matrix_t *Ai;

        diag[0] = num_create_from_long(2);
        diag[1] = num_create_from_string("3/2");
        A = mat_create_diagonal_num(2, diag);
        det = num_new();
        check_bool("mat_create_diagonal_num(exact) non-null", A != NULL);
        check_bool("mat_det(number exact diagonal) rc = 0", A && mat_det(A, &det) == 0);
        expected = num_create_from_long(3);
        check_bool("mat_det(number exact diagonal) stays exact", num_eq(det, expected));
        num_destroy(&expected);
        num_destroy(&det);

        Ai = mat_inverse(A);
        check_bool("mat_inverse(number exact diagonal) not NULL", Ai != NULL);
        check_bool("mat_inverse(number exact diagonal) -> MAT_TYPE_NUMBER",
                   Ai != NULL && mat_typeof(Ai) == MAT_TYPE_NUMBER);

        got = mat_get_num(Ai, 0, 0);
        expected = num_create_from_string("1/2");
        check_bool("inverse exact [0,0] = 1/2", num_eq(got, expected));
        num_destroy(&expected);
        num_destroy(&got);

        got = mat_get_num(Ai, 1, 1);
        expected = num_create_from_string("2/3");
        check_bool("inverse exact [1,1] = 2/3", num_eq(got, expected));
        num_destroy(&expected);
        num_destroy(&got);

        mat_free(Ai);
        mat_free(A);
        num_destroy(&diag[0]);
        num_destroy(&diag[1]);
    }

    {
        mfloat_t *a_base = mf_create_string("1.25");
        mfloat_t *b_base = mf_create_string("2.5");
        number_t diag[2];
        number_t det = num_new();
        number_t expected;
        number_t got;
        matrix_t *A;
        matrix_t *Ai;

        check_bool("number inverse/det mfloat base a non-null", a_base != NULL);
        check_bool("number inverse/det mfloat base b non-null", b_base != NULL);
        diag[0] = num_create_from_mfloat_with_prec_bits(a_base, 512u);
        diag[1] = num_create_from_mfloat_with_prec_bits(b_base, 512u);
        A = mat_create_diagonal_num(2, diag);

        check_bool("mat_det(number mp diagonal) rc = 0", A && mat_det(A, &det) == 0);
        expected = num_mul(diag[0], diag[1]);
        check_bool("mat_det(number mp diagonal) matches", num_eq(det, expected));
        check_bool("mat_det(number mp diagonal) does not lose precision",
                   num_get_prec_bits(det) >= num_get_prec_bits(diag[0]) &&
                   num_get_prec_bits(det) >= num_get_prec_bits(diag[1]));
        print_matrix_core_num_comparison("det(number diagonal)", det, expected);
        num_destroy(&expected);
        num_destroy(&det);

        Ai = mat_inverse(A);
        check_bool("mat_inverse(number mp diagonal) not NULL", Ai != NULL);
        check_bool("mat_inverse(number mp diagonal) -> MAT_TYPE_NUMBER",
                   Ai != NULL && mat_typeof(Ai) == MAT_TYPE_NUMBER);

        got = mat_get_num(Ai, 0, 0);
        expected = num_inv(diag[0]);
        check_bool("mat_inverse(number mp diagonal)[0,0] matches", num_eq(got, expected));
        check_bool("mat_inverse(number mp diagonal)[0,0] preserves precision",
                   num_get_prec_bits(got) == 512u);
        print_matrix_core_num_comparison("inverse(number diagonal)[0,0]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);

        mat_free(Ai);
        mat_free(A);
        num_destroy(&diag[0]);
        num_destroy(&diag[1]);
        mf_free(a_base);
        mf_free(b_base);
    }
}

static void test_mixed_number_backend_matrices(void)
{
    printf(C_CYAN "TEST: mixed number_t backend matrices\n" C_RESET);

    {
        mfloat_t *real_base = mf_create_string("1.25");
        mcomplex_t *complex_base = mc_create_string("1 + 2i");
        number_t vals[4];
        number_t doubled_expected;
        number_t got;
        matrix_t *A;
        matrix_t *B;

        check_bool("mixed number matrix real base non-null", real_base != NULL);
        check_bool("mixed number matrix complex base non-null", complex_base != NULL);

        vals[0] = num_create_from_long(2);
        vals[1] = num_create_from_string("1/3");
        vals[2] = num_create_from_mfloat_with_prec_bits(real_base, 512u);
        vals[3] = num_create_from_mcomplex_with_prec_bits(complex_base, 384u);

        A = mat_create_num(2, 2, vals);
        check_bool("mixed number matrix create non-null", A != NULL);
        check_bool("mixed number matrix type", A != NULL && mat_typeof(A) == MAT_TYPE_NUMBER);

        got = mat_get_num(A, 0, 0);
        check_bool("mixed number matrix integer entry", num_eq(got, vals[0]));
        num_destroy(&got);

        got = mat_get_num(A, 0, 1);
        check_bool("mixed number matrix rational entry", num_eq(got, vals[1]));
        num_destroy(&got);

        got = mat_get_num(A, 1, 0);
        check_bool("mixed number matrix mfloat entry", num_eq(got, vals[2]));
        check_bool("mixed number matrix mfloat precision", num_get_prec_bits(got) == 512u);
        num_destroy(&got);

        got = mat_get_num(A, 1, 1);
        check_bool("mixed number matrix mcomplex entry", num_eq(got, vals[3]));
        check_bool("mixed number matrix mcomplex precision", num_get_prec_bits(got) == 384u);
        num_destroy(&got);

        B = mat_add(A, A);
        check_bool("mixed number matrix add non-null", B != NULL);
        check_bool("mixed number matrix add type", B != NULL && mat_typeof(B) == MAT_TYPE_NUMBER);

        got = mat_get_num(B, 0, 0);
        doubled_expected = num_add(vals[0], vals[0]);
        check_bool("mixed number add integer entry", num_eq(got, doubled_expected));
        num_destroy(&doubled_expected);
        num_destroy(&got);

        got = mat_get_num(B, 0, 1);
        doubled_expected = num_add(vals[1], vals[1]);
        check_bool("mixed number add rational entry", num_eq(got, doubled_expected));
        num_destroy(&doubled_expected);
        num_destroy(&got);

        got = mat_get_num(B, 1, 0);
        doubled_expected = num_add(vals[2], vals[2]);
        check_bool("mixed number add mfloat entry", num_eq(got, doubled_expected));
        check_bool("mixed number add mfloat precision does not shrink",
                   num_get_prec_bits(got) >= num_get_prec_bits(vals[2]));
        print_matrix_core_num_comparison("mixed add [1,0]", got, doubled_expected);
        num_destroy(&doubled_expected);
        num_destroy(&got);

        got = mat_get_num(B, 1, 1);
        doubled_expected = num_add(vals[3], vals[3]);
        check_bool("mixed number add mcomplex entry", num_eq(got, doubled_expected));
        check_bool("mixed number add mcomplex precision does not shrink",
                   num_get_prec_bits(got) >= num_get_prec_bits(vals[3]));
        print_matrix_core_num_comparison("mixed add [1,1]", got, doubled_expected);
        num_destroy(&doubled_expected);
        num_destroy(&got);

        mat_free(B);
        mat_free(A);
        for (size_t i = 0; i < 4u; ++i)
            num_destroy(&vals[i]);
        mf_free(real_base);
        mc_free(complex_base);
    }

    {
        mfloat_t *real_base = mf_create_string("1.25");
        mcomplex_t *complex_base = mc_create_string("1 + 2i");
        number_t diag[4];
        number_t det = num_new();
        number_t expected;
        number_t got;
        matrix_t *A;
        matrix_t *Ai;

        check_bool("mixed diagonal real base non-null", real_base != NULL);
        check_bool("mixed diagonal complex base non-null", complex_base != NULL);

        diag[0] = num_create_from_long(2);
        diag[1] = num_create_from_string("1/3");
        diag[2] = num_create_from_mfloat_with_prec_bits(real_base, 512u);
        diag[3] = num_create_from_mcomplex_with_prec_bits(complex_base, 384u);

        A = mat_create_diagonal_num(4, diag);
        check_bool("mixed diagonal create non-null", A != NULL);

        check_bool("mixed diagonal det rc = 0", A && mat_det(A, &det) == 0);
        expected = num_mul(num_mul(diag[0], diag[1]), num_mul(diag[2], diag[3]));
        check_bool("mixed diagonal det matches", num_eq(det, expected));
        check_bool("mixed diagonal det precision does not shrink",
                   num_get_prec_bits(det) >= num_get_prec_bits(diag[2]) &&
                   num_get_prec_bits(det) >= num_get_prec_bits(diag[3]));
        print_matrix_core_num_comparison("mixed det(diagonal)", det, expected);
        num_destroy(&expected);
        num_destroy(&det);

        Ai = mat_inverse(A);
        check_bool("mixed diagonal inverse non-null", Ai != NULL);
        check_bool("mixed diagonal inverse type", Ai != NULL && mat_typeof(Ai) == MAT_TYPE_NUMBER);

        got = mat_get_num(Ai, 1, 1);
        expected = num_inv(diag[1]);
        check_bool("mixed diagonal inverse rational entry", num_eq(got, expected));
        num_destroy(&expected);
        num_destroy(&got);

        got = mat_get_num(Ai, 2, 2);
        expected = num_inv(diag[2]);
        check_bool("mixed diagonal inverse mfloat entry", num_eq(got, expected));
        check_bool("mixed diagonal inverse mfloat precision", num_get_prec_bits(got) == 512u);
        print_matrix_core_num_comparison("mixed inverse [2,2]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);

        got = mat_get_num(Ai, 3, 3);
        expected = num_inv(diag[3]);
        check_bool("mixed diagonal inverse mcomplex entry", num_eq(got, expected));
        check_bool("mixed diagonal inverse mcomplex precision", num_get_prec_bits(got) >= 384u);
        print_matrix_core_num_comparison("mixed inverse [3,3]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);

        mat_free(Ai);
        mat_free(A);
        for (size_t i = 0; i < 4u; ++i)
            num_destroy(&diag[i]);
        mf_free(real_base);
        mc_free(complex_base);
    }
}

static void test_dval_multiply(void)
{
    printf(C_CYAN "TEST: dval matrix multiply\n" C_RESET);

    dval_t *x = test_dv_new_named_var_d(2.0, "x");
    dval_t *y = test_dv_new_named_var_d(4.0, "y");
    dval_t *one = test_dv_new_const_d(1.0);
    dval_t *three = test_dv_new_const_d(3.0);

    dval_t *avals[2] = {x, one};
    dval_t *bvals[2] = {three, y};

    matrix_t *A = mat_create_dv(1, 2, avals);
    matrix_t *B = mat_create_dv(2, 1, bvals);
    matrix_t *C = mat_mul(A, B);
    dval_t *out = NULL;

    check_bool("mat_create_dv(A) non-null", A != NULL);
    check_bool("mat_create_dv(B) non-null", B != NULL);
    check_bool("mat_mul(dval,dval) non-null", C != NULL);
    check_bool("mat_mul(dval,dval) -> MAT_TYPE_DVAL",
               C != NULL && mat_typeof(C) == MAT_TYPE_DVAL);

    if (A)
        print_mdv("A", A);
    if (B)
        print_mdv("B", B);
    if (C)
        print_mdv("A*B", C);

    if (C) {
        mat_get(C, 0, 0, &out);
        check_d("C[0,0] = 3*x + y at x=2,y=4", dv_eval_d(out), 10.0, 1e-12);

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 7.0);
        check_d("C[0,0] tracks updated variables", dv_eval_d(out), 22.0, 1e-12);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
    dv_free(x);
    dv_free(y);
    dv_free(one);
    dv_free(three);
}

static void test_dval_symbolic_printing(void)
{
    printf(C_CYAN "TEST: dval symbolic matrix printing\n" C_RESET);

    dval_t *x = test_dv_new_named_var_d(0.0, "x");
    dval_t *y = test_dv_new_named_var_d(1.0, "y");
    dval_t *z = test_dv_new_named_var_d(2.0, "z");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");
    dval_t *alpha = test_dv_new_named_const_d(3.1415926535897932384626433, "@alpha");
    dval_t *cos_y = dv_cos(y);

    dval_t *a00 = dv_mul(pi, cos_y);
    dval_t *a01 = DV_ONE;
    dval_t *a10 = dv_tan(z);
    dval_t *a11 = dv_exp(y);

    dval_t *b00 = alpha;
    dval_t *b01 = tau;
    dval_t *b10 = x;
    dval_t *b11 = dv_add(alpha, x);

    dval_t *avals[4] = {a00, a01, a10, a11};
    dval_t *bvals[4] = {b00, b01, b10, b11};

    matrix_t *A = mat_create_dv(2, 2, avals);
    matrix_t *B = mat_create_dv(2, 2, bvals);

    check_bool("mat_create_dv(symbolic A) non-null", A != NULL);
    check_bool("mat_create_dv(symbolic B) non-null", B != NULL);

    if (A)
        print_mdv("A", A);
    if (B)
        print_mdv("B", B);

    mat_free(A);
    mat_free(B);

    dv_free(a00);
    dv_free(cos_y);
    dv_free(a10);
    dv_free(a11);
    dv_free(a01);
    dv_free(b11);
    dv_free(x);
    dv_free(y);
    dv_free(z);
    dv_free(pi);
    dv_free(tau);
    dv_free(alpha);
}

static void check_dval_text_contains(const char *label, dval_t *dv, const char *needle)
{
    char *s = dv ? dv_to_string(dv, style_EXPRESSION) : NULL;
    check_bool(label, s && strstr(s, needle) != NULL);
    free(s);
}

static void print_det_dv(const char *label, dval_t *dv)
{
    char *s = dv ? dv_to_string(dv, style_EXPRESSION) : NULL;
    printf("      %s = %s\n", label, s ? s : "<null>");
    free(s);
}

/* ------------------------------------------------------------------ 4. add/sub (double only) */

static void test_add_sub(void)
{
    printf(C_CYAN "TEST: addition/subtraction\n" C_RESET);

    const double A_vals[4] = {1, 2, 3, 4};
    const double B_vals[4] = {5, 6, 7, 8};

    matrix_t *A = test_mat_create_d(2, 2, A_vals);
    matrix_t *B = test_mat_create_d(2, 2, B_vals);

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
    matrix_t *A = test_mat_create_d(2, 2, A_vals);
    matrix_t *B = test_mat_create_d(2, 2, B_vals);

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

    matrix_t *A = test_mat_dense_d(2, 3);
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

    matrix_t *C = test_mat_dense_qc(2, 2);
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

    matrix_t *I = test_mat_identity_d(3);
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

    matrix_t *I = test_mat_identity_d(3);

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

static void test_owned_element_reads_and_transforms(void)
{
    printf(C_CYAN "TEST: owned element reads and transforms\n" C_RESET);

    number_t x0 = num_create_from_double(2.0);
    dval_t *x = dv_new_named_var_num(x0, "x");
    dval_t *one = DV_ONE;
    dval_t *owned_x = NULL;
    dval_t *owned_one = NULL;
    matrix_t *A = mat_new_dv(1, 2);
    matrix_t *T = NULL;
    dval_t *t_entry = NULL;

    num_destroy(&x0);

    check_bool("mat_new_dv owned-read source non-null", A != NULL);
    if (!A || !x)
        goto cleanup;

    mat_set(A, 0, 0, &x);
    mat_set(A, 0, 1, &one);

    mat_value_init_zero(A, &owned_x);
    mat_value_init_zero(A, &owned_one);

    mat_get_owned(A, 0, 0, &owned_x);
    mat_get_owned(A, 0, 1, &owned_one);

    check_bool("mat_get_owned dval variable non-null", owned_x != NULL);
    check_bool("mat_get_owned dval constant non-null", owned_one != NULL);
    check_d("owned x read evaluates", dv_eval_d(owned_x), 2.0, 1e-30);
    check_d("owned one read evaluates", dv_eval_d(owned_one), 1.0, 1e-30);

    dv_free(owned_x);
    owned_x = NULL;
    dv_free(owned_one);
    owned_one = NULL;

    test_dv_set_val_d(x, 4.5);
    {
        dval_t *entry = NULL;
        mat_get(A, 0, 0, &entry);
        check_d("matrix entry survives owned read destruction", dv_eval_d(entry), 4.5, 1e-30);
    }

    T = mat_transpose(A);
    check_bool("mat_transpose(dval owned-read source) non-null", T != NULL);
    mat_free(A);
    A = NULL;

    if (!T)
        goto cleanup;

    mat_get(T, 0, 0, &t_entry);
    check_bool("transposed dval entry non-null", t_entry != NULL);
    check_d("transposed dval entry tracks x after source free", dv_eval_d(t_entry), 4.5, 1e-30);

cleanup:
    if (owned_x)
        dv_free(owned_x);
    if (owned_one)
        dv_free(owned_one);
    mat_value_destroy(A ? A : T, &owned_x);
    mat_value_destroy(A ? A : T, &owned_one);
    mat_free(T);
    mat_free(A);
    dv_free(x);
}

static void test_dval_storage_lifecycle_regressions(void)
{
    printf(C_CYAN "TEST: dval storage lifecycle regressions\n" C_RESET);

    {
        matrix_t *A = mat_new_dv(2, 2);
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *entry = NULL;

        check_bool("mat_new_dv dense non-null", A != NULL);
        if (A) {
            mat_set(A, 0, 0, &x);
            mat_get(A, 0, 0, &entry);
            check_d("dense dval slot stores x", dv_eval_d(entry), 2.0, 1e-12);

            test_dv_set_val_d(x, 3.5);
            check_d("dense dval slot tracks x", dv_eval_d(entry), 3.5, 1e-12);
        }

        mat_free(A);
        dv_free(x);
    }

    {
        dval_t *diag_vals[2];
        matrix_t *D;
        dval_t *x = test_dv_new_named_var_d(4.0, "x");
        dval_t *y = test_dv_new_named_var_d(5.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *entry = NULL;

        diag_vals[0] = x;
        diag_vals[1] = y;
        D = mat_create_diagonal_dv(2, diag_vals);

        check_bool("mat_create_diagonal_dv non-null", D != NULL);
        if (D) {
            mat_set(D, 0, 1, &one);

            mat_get(D, 0, 0, &entry);
            check_d("diagonal materialise preserves [0,0]", dv_eval_d(entry), 4.0, 1e-12);
            mat_get(D, 1, 1, &entry);
            check_d("diagonal materialise preserves [1,1]", dv_eval_d(entry), 5.0, 1e-12);
            mat_get(D, 0, 1, &entry);
            check_d("diagonal materialise sets off-diagonal", dv_eval_d(entry), 1.0, 1e-12);
        }

        mat_free(D);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        matrix_t *I = mat_create_identity_dv(2);
        dval_t *x = test_dv_new_named_var_d(6.0, "x");
        dval_t *entry = NULL;

        check_bool("mat_create_identity_dv non-null", I != NULL);
        if (I) {
            mat_set(I, 0, 1, &x);

            mat_get(I, 0, 0, &entry);
            check_d("identity materialise preserves [0,0]", dv_eval_d(entry), 1.0, 1e-12);
            mat_get(I, 1, 1, &entry);
            check_d("identity materialise preserves [1,1]", dv_eval_d(entry), 1.0, 1e-12);
            mat_get(I, 0, 1, &entry);
            check_d("identity materialise sets off-diagonal", dv_eval_d(entry), 6.0, 1e-12);

            test_dv_set_val_d(x, 8.0);
            check_d("identity materialise tracks x", dv_eval_d(entry), 8.0, 1e-12);
        }

        mat_free(I);
        dv_free(x);
    }
}

static void test_sparse_support(void)
{
    printf(C_CYAN "TEST: sparse storage support\n" C_RESET);

    {
        matrix_t *S = test_mat_sparse_d(3, 3);
        double zero = 0.0, five = 5.0, minus_two = -2.0;
        double got = 0.0;
        matrix_t *D = NULL;
        matrix_t *Expected = NULL;

        check_bool("mat_new_sparse_d non-null", S != NULL);
        check_bool("new sparse matrix reports sparse", mat_is_sparse(S));
        check_bool("new sparse matrix nnz = 0", mat_nonzero_count(S) == 0);

        mat_set(S, 0, 2, &five);
        mat_set(S, 2, 1, &minus_two);
        check_bool("sparse matrix nnz after two inserts = 2", mat_nonzero_count(S) == 2);
        mat_get(S, 0, 2, &got);
        check_d("sparse get S[0,2] = 5", got, 5.0, 1e-12);
        mat_get(S, 2, 1, &got);
        check_d("sparse get S[2,1] = -2", got, -2.0, 1e-12);

        mat_set(S, 0, 2, &zero);
        check_bool("setting zero removes sparse entry", mat_nonzero_count(S) == 1);
        mat_get(S, 0, 2, &got);
        check_d("removed sparse entry reads as zero", got, 0.0, 1e-12);

        D = mat_to_dense(S);
        Expected = test_mat_create_d(3, 3, (double[9]){
            0.0, 0.0, 0.0,
            0.0, 0.0, 0.0,
            0.0, -2.0, 0.0
        });
        check_bool("mat_to_dense(sparse) not NULL", D != NULL);
        if (D)
            check_mat_d("dense(sparse) matches expected", D, Expected, 1e-12);

        mat_free(D);
        mat_free(Expected);
        mat_free(S);
    }

    {
        matrix_t *A = test_mat_create_d(3, 3, (double[9]){
            1.0, 0.0, 0.0,
            0.0, 0.0, 2.0,
            0.0, 0.0, 3.0
        });
        matrix_t *S = mat_to_sparse(A);
        matrix_t *B = test_mat_create_d(3, 1, (double[3]){4.0, 5.0, 6.0});
        matrix_t *SB = NULL;
        matrix_t *Expected = test_mat_create_d(3, 1, (double[3]){4.0, 12.0, 18.0});
        matrix_t *Back = NULL;

        print_md("A", A);
        check_bool("mat_to_sparse(dense) not NULL", S != NULL);
        check_bool("converted matrix reports sparse", S && mat_is_sparse(S));
        check_bool("converted matrix nnz = 3", S && mat_nonzero_count(S) == 3);

        if (S) {
            Back = mat_to_dense(S);
            check_bool("dense round-trip not NULL", Back != NULL);
            if (Back)
                check_mat_d("dense->sparse->dense = original", Back, A, 1e-12);

            SB = mat_mul(S, B);
            check_bool("sparse * dense vector not NULL", SB != NULL);
            if (SB)
                check_mat_d("sparse matmul gives correct result", SB, Expected, 1e-12);
        }

        mat_free(Back);
        mat_free(SB);
        mat_free(Expected);
        mat_free(B);
        mat_free(S);
        mat_free(A);
    }

    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        matrix_t *B = test_mat_sparse_d(3, 3);
        matrix_t *Sum = NULL;
        matrix_t *Diff = NULL;
        matrix_t *Prod = NULL;
        matrix_t *ExpectedSum = NULL;
        matrix_t *ExpectedProd = NULL;
        double a00 = 1.0, a12 = 2.0, b00 = -1.0, b21 = 3.0, b22 = 4.0;

        check_bool("sparse add/sub inputs non-null", A != NULL && B != NULL);
        if (A && B) {
            mat_set(A, 0, 0, &a00);
            mat_set(A, 1, 2, &a12);
            mat_set(B, 0, 0, &b00);
            mat_set(B, 2, 1, &b21);
            mat_set(B, 2, 2, &b22);

            Sum = mat_add(A, B);
            Diff = mat_sub(A, A);
            Prod = mat_mul(A, B);

            ExpectedSum = test_mat_create_d(3, 3, (double[9]){
                0.0, 0.0, 0.0,
                0.0, 0.0, 2.0,
                0.0, 3.0, 4.0
            });
            ExpectedProd = test_mat_create_d(3, 3, (double[9]){
                -1.0, 0.0, 0.0,
                0.0, 6.0, 8.0,
                0.0, 0.0, 0.0
            });

            check_bool("sparse + sparse not NULL", Sum != NULL);
            check_bool("sparse + sparse stays sparse", Sum && mat_is_sparse(Sum));
            if (Sum)
                check_mat_d("sparse + sparse matches expected", Sum, ExpectedSum, 1e-12);

            check_bool("sparse - sparse not NULL", Diff != NULL);
            check_bool("sparse - sparse stays sparse", Diff && mat_is_sparse(Diff));
            check_bool("sparse - self has nnz = 0", Diff && mat_nonzero_count(Diff) == 0);

            check_bool("sparse * sparse not NULL", Prod != NULL);
            check_bool("sparse * sparse stays sparse", Prod && mat_is_sparse(Prod));
            if (Prod)
                check_mat_d("sparse * sparse matches expected", Prod, ExpectedProd, 1e-12);
        }

        mat_free(ExpectedProd);
        mat_free(ExpectedSum);
        mat_free(Prod);
        mat_free(Diff);
        mat_free(Sum);
        mat_free(B);
        mat_free(A);
    }

    {
        matrix_t *I = test_mat_identity_d(3);
        matrix_t *S = test_mat_sparse_d(3, 3);
        matrix_t *L = NULL;
        matrix_t *R = NULL;
        matrix_t *Expected = NULL;
        double s01 = 2.0, s22 = -5.0;

        check_bool("identity and sparse inputs non-null", I != NULL && S != NULL);
        if (I && S) {
            mat_set(S, 0, 1, &s01);
            mat_set(S, 2, 2, &s22);
            Expected = test_mat_create_d(3, 3, (double[9]){
                0.0, 2.0, 0.0,
                0.0, 0.0, 0.0,
                0.0, 0.0, -5.0
            });

            L = mat_mul(I, S);
            R = mat_mul(S, I);

            check_bool("identity * sparse not NULL", L != NULL);
            check_bool("identity * sparse stays sparse", L && mat_is_sparse(L));
            if (L)
                check_mat_d("identity * sparse = sparse", L, Expected, 1e-12);

            check_bool("sparse * identity not NULL", R != NULL);
            check_bool("sparse * identity stays sparse", R && mat_is_sparse(R));
            if (R)
                check_mat_d("sparse * identity = sparse", R, Expected, 1e-12);
        }

        mat_free(Expected);
        mat_free(R);
        mat_free(L);
        mat_free(S);
        mat_free(I);
    }

    {
        qcomplex_t vals[4] = {
            qc_make(qf_from_double(0.0), QF_ZERO),
            qc_make(qf_from_double(2.0), qf_from_double(-1.0)),
            QC_ZERO,
            qc_make(qf_from_double(0.0), QF_ZERO)
        };
        qcomplex_t zero = QC_ZERO;
        matrix_t *S = test_mat_sparse_qc(2, 2);
        matrix_t *D = NULL;
        matrix_t *Expected = test_mat_create_qc(2, 2, vals);

        check_bool("mat_new_sparse_qc non-null", S != NULL);
        if (S) {
            mat_set(S, 0, 1, &vals[1]);
            check_bool("qcomplex sparse nnz = 1", mat_nonzero_count(S) == 1);
            mat_set(S, 1, 1, &zero);
            check_bool("setting qcomplex zero leaves nnz unchanged", mat_nonzero_count(S) == 1);
            D = mat_to_dense(S);
            check_bool("dense(qcomplex sparse) not NULL", D != NULL);
            if (D)
                check_mat_qc("qcomplex sparse round-trip", D, Expected, 1e-25);
        }

        mat_free(D);
        mat_free(Expected);
        mat_free(S);
    }
}

static void test_layout_policy_regressions(void)
{
    printf(C_CYAN "TEST: layout policy regressions\n" C_RESET);

    {
        matrix_t *S = test_mat_sparse_d(2, 2);
        matrix_t *D = test_mat_create_d(2, 2, (double[4]){
            10.0, 20.0,
            30.0, 40.0
        });
        matrix_t *R = NULL;
        matrix_t *Expected = test_mat_create_d(2, 2, (double[4]){
            11.0, 20.0,
            30.0, 38.0
        });
        double one = 1.0, minus_two = -2.0;

        check_bool("dense+sparse inputs allocated", S != NULL && D != NULL && Expected != NULL);
        if (S && D && Expected) {
            mat_set(S, 0, 0, &one);
            mat_set(S, 1, 1, &minus_two);

            R = mat_add(D, S);
            check_bool("dense + sparse not NULL", R != NULL);
            check_bool("dense + sparse falls back to dense", R && !mat_is_sparse(R));
            if (R)
                check_mat_d("dense + sparse matches expected", R, Expected, 1e-12);
        }

        mat_free(Expected);
        mat_free(R);
        mat_free(D);
        mat_free(S);
    }

    {
        matrix_t *I = test_mat_identity_d(3);
        matrix_t *S = test_mat_sparse_d(3, 3);
        matrix_t *R = NULL;
        matrix_t *Expected = test_mat_create_d(3, 3, (double[9]){
            1.0, 0.0, 0.0,
            0.0, -2.0, 0.0,
            0.0, 0.0, 0.5
        });
        double three = 3.0, half = 0.5;

        check_bool("identity-sparse subtraction inputs allocated",
                   I != NULL && S != NULL && Expected != NULL);
        if (I && S && Expected) {
            mat_set(S, 1, 1, &three);
            mat_set(S, 2, 2, &half);

            R = mat_sub(I, S);
            check_bool("identity - sparse not NULL", R != NULL);
            check_bool("identity - sparse stays sparse-like", R && mat_is_sparse(R));
            if (R)
                check_mat_d("identity - sparse matches expected", R, Expected, 1e-12);
        }

        mat_free(Expected);
        mat_free(R);
        mat_free(S);
        mat_free(I);
    }

    {
        matrix_t *I = test_mat_identity_d(3);
        matrix_t *N = NULL;
        matrix_t *Expected = test_mat_create_d(3, 3, (double[9]){
            -1.0, 0.0, 0.0,
             0.0, -1.0, 0.0,
             0.0, 0.0, -1.0
        });

        check_bool("identity negation input allocated", I != NULL && Expected != NULL);
        if (I && Expected) {
            N = mat_neg(I);
            check_bool("mat_neg(identity) not NULL", N != NULL);
            check_bool("mat_neg(identity) preserves diagonal structure", N && mat_is_diagonal(N));
            if (N)
                check_mat_d("mat_neg(identity) = -I", N, Expected, 1e-12);
        }

        mat_free(Expected);
        mat_free(N);
        mat_free(I);
    }

    {
        matrix_t *S = test_mat_sparse_d(2, 3);
        matrix_t *R = NULL;
        matrix_t *Expected = NULL;
        double three = 3.0;
        double two = 2.0, minus_four = -4.0;

        check_bool("sparse scalar-multiply input allocated", S != NULL);
        if (S) {
            number_t expected_vals[6];

            expected_vals[0] = num_create_from_long(0);
            expected_vals[1] = num_create_from_long(-4);
            expected_vals[2] = num_create_from_long(0);
            expected_vals[3] = num_create_from_long(8);
            expected_vals[4] = num_create_from_long(0);
            expected_vals[5] = num_create_from_long(-6);
            Expected = mat_create_num(2, 3, expected_vals);
            for (size_t idx = 0; idx < 6; ++idx)
                num_destroy(&expected_vals[idx]);

            check_bool("sparse scalar-multiply expected allocated", Expected != NULL);
        }
        if (S && Expected) {
            mat_set(S, 0, 1, &two);
            mat_set(S, 1, 0, &minus_four);
            mat_set(S, 1, 2, &three);

            {
                number_t minus_two = num_create_from_double(-2.0);
                R = mat_scalar_mul_num(S, &minus_two);
                num_destroy(&minus_two);
            }
            check_bool("scalar multiply of sparse not NULL", R != NULL);
            check_bool("scalar multiply of sparse stays sparse-like", R && mat_is_sparse(R));
            check_bool("scalar multiply of sparse promotes to number",
                       R && mat_typeof(R) == MAT_TYPE_NUMBER);
            if (R) {
                int matches = 1;

                for (size_t i = 0; i < 2 && matches; ++i)
                    for (size_t j = 0; j < 3; ++j) {
                        number_t got = mat_get_num(R, i, j);
                        number_t want = mat_get_num(Expected, i, j);

                        if (!num_eq(got, want))
                            matches = 0;
                        num_destroy(&got);
                        num_destroy(&want);
                        if (!matches)
                            break;
                    }

                check_bool("scalar multiply of sparse matches expected", matches);
            }
        }

        mat_free(Expected);
        mat_free(R);
        mat_free(S);
    }

    {
        matrix_t *S = test_mat_sparse_d(2, 3);
        matrix_t *T = NULL;
        matrix_t *Expected = test_mat_create_d(3, 2, (double[6]){
            0.0, 4.0,
            5.0, 0.0,
            0.0, 0.0
        });
        double five = 5.0, four = 4.0;

        check_bool("sparse transpose inputs allocated",
                   S != NULL && Expected != NULL);
        if (S && Expected) {
            mat_set(S, 0, 1, &five);
            mat_set(S, 1, 0, &four);

            T = mat_transpose(S);
            check_bool("transpose of sparse not NULL", T != NULL);
            check_bool("transpose of sparse stays sparse-like", T && mat_is_sparse(T));
            if (T)
                check_mat_d("transpose of sparse matches expected", T, Expected, 1e-12);
        }

        mat_free(Expected);
        mat_free(T);
        mat_free(S);
    }

    {
        matrix_t *I = test_mat_identity_qc(2);
        matrix_t *C = NULL;
        qcomplex_t expected_vals[4] = {
            qc_make(qf_from_double(1.0), QF_ZERO), QC_ZERO,
            QC_ZERO, qc_make(qf_from_double(1.0), QF_ZERO)
        };
        matrix_t *Expected = test_mat_create_qc(2, 2, expected_vals);

        check_bool("identity conjugate inputs allocated",
                   I != NULL && Expected != NULL);
        if (I && Expected) {
            C = mat_conj(I);
            check_bool("conjugate of identity not NULL", C != NULL);
            check_bool("conjugate of identity preserves diagonal structure", C && mat_is_diagonal(C));
            if (C)
                check_mat_qc("conjugate of identity matches expected", C, Expected, 1e-25);
        }

        mat_free(Expected);
        mat_free(C);
        mat_free(I);
    }
}

static void test_structural_queries_and_diagonal_construction(void)
{
    printf(C_CYAN "TEST: structural queries and diagonal construction\n" C_RESET);

    {
        double diag_vals[3] = {2.0, -1.0, 0.5};
        matrix_t *D = test_mat_diagonal_d(3, diag_vals);
        double seven = 7.0;

        check_bool("mat_create_diagonal_d not NULL", D != NULL);
        check_bool("diagonal matrix recognised as diagonal", D && mat_is_diagonal(D));
        check_bool("diagonal matrix recognised as upper triangular", D && mat_is_upper_triangular(D));
        check_bool("diagonal matrix recognised as lower triangular", D && mat_is_lower_triangular(D));
        check_bool("diagonal matrix is not sparse storage", D && !mat_is_sparse(D));
        check_bool("diagonal nonzero count = 3", D && mat_nonzero_count(D) == 3);

        if (D) {
            mat_set(D, 0, 1, &seven);
            check_bool("off-diagonal write breaks diagonal structure", !mat_is_diagonal(D));
            check_bool("off-diagonal write preserves upper-triangular structure", mat_is_upper_triangular(D));
            check_bool("off-diagonal write breaks lower-triangular structure", !mat_is_lower_triangular(D));
        }

        mat_free(D);
    }

    {
        qcomplex_t diag_vals[2] = {
            qc_make(qf_from_double(1.0), qf_from_double(2.0)),
            qc_make(qf_from_double(-3.0), qf_from_double(0.5))
        };
        matrix_t *D = test_mat_diagonal_qc(2, diag_vals);

        check_bool("mat_create_diagonal_qc not NULL", D != NULL);
        check_bool("qcomplex diagonal recognised as diagonal", D && mat_is_diagonal(D));
        check_bool("qcomplex diagonal nonzero count = 2", D && mat_nonzero_count(D) == 2);

        mat_free(D);
    }
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

    matrix_t *A = test_mat_create_qf(2, 3, a_vals);
    matrix_t *B = test_mat_create_qf(2, 3, b_vals);

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
    matrix_t *A = test_mat_create_qc(1, 3, a_vals);
    matrix_t *B = test_mat_create_qc(1, 3, b_vals);

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

    matrix_t *A = test_mat_create_qf(2, 3, A_vals);
    matrix_t *B = test_mat_create_qf(3, 2, B_vals);

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

    matrix_t *A = test_mat_create_qc(3, 1, a_vals);
    matrix_t *B = test_mat_create_qc(1, 4, b_vals);

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

    matrix_t *A = test_mat_create_d(2, 2, a_vals);
    matrix_t *B = test_mat_create_qf(2, 2, b_vals);

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

    matrix_t *A = test_mat_create_d(1, 3, a_vals);
    matrix_t *B = test_mat_create_qc(1, 3, b_vals);

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

    matrix_t *A = test_mat_create_qf(2, 1, a_vals);
    matrix_t *B = test_mat_create_qc(2, 1, b_vals);

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

    matrix_t *A = test_mat_create_d(2, 2, a_vals);
    matrix_t *B = test_mat_create_qf(2, 2, b_vals);

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

    matrix_t *A = test_mat_create_d(1, 3, a_vals);
    matrix_t *B = test_mat_create_qc(1, 3, b_vals);

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

    matrix_t *A = test_mat_create_qf(2, 1, a_vals);
    matrix_t *B = test_mat_create_qc(2, 1, b_vals);

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

    matrix_t *A = test_mat_create_d(2, 3, a_vals);
    matrix_t *B = test_mat_create_qf(3, 2, b_vals);

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

    matrix_t *A = test_mat_create_d(1, 3, a_vals);
    matrix_t *B = test_mat_create_qc(3, 2, b_vals);

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

    matrix_t *A = test_mat_create_qf(2, 1, a_vals);
    matrix_t *B = test_mat_create_qc(1, 3, b_vals);

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
    number_t alpha_num = num_create_from_double(alpha);

    matrix_t *A = test_mat_create_d(2, 2, A_vals);
    print_md("A", A);

    matrix_t *B = mat_scalar_mul_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("alpha * A", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_double(alpha * A_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*d [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_double(alpha);

    matrix_t *A = test_mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_mul_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("alpha * A (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_qfloat(
            qf_mul(qf_from_double(alpha), A_vals[k]));
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*qf [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_double(alpha);

    matrix_t *A = test_mat_create_qc(1, 3, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_mul_num(A, &alpha_num);
    number_t B_vals[3];
    print_mnum("alpha * A (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t j = 0; j < 3; j++)
    {
        number_t expected = num_create_from_qcomplex(qc_mul(
            qc_make(qf_from_double(alpha), QF_ZERO),
            A_vals[j]));
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*qc [0,%zu]", j);
        check_bool(label, num_eq(B_vals[j], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[j]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_qfloat(alpha);

    matrix_t *A = test_mat_create_d(2, 3, A_vals);
    print_md("A (double)", A);

    matrix_t *B = mat_scalar_mul_num(A, &alpha_num);
    number_t B_vals[6];
    print_mnum("alpha * A (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 6; k++)
    {
        number_t expected = num_create_from_qfloat(
            qf_mul(alpha, qf_from_double(A_vals[k])));
        char label[64];
        snprintf(label, sizeof(label), "scalar mul qf*d [%zu,%zu]", k / 3, k % 3);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_qcomplex(alpha);

    matrix_t *A = test_mat_create_qc(2, 2, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_mul_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("alpha * A (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_qcomplex(qc_mul(alpha, A_vals[k]));
        char label[64];
        snprintf(label, sizeof(label), "scalar mul qc*qc [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ identity add/sub/mul: double */

static void test_identity_arith_d(void)
{
    printf(C_CYAN "TEST: identity arithmetic (double)\n" C_RESET);

    double vals[4] = {1, 2, 3, 4};
    matrix_t *A = test_mat_create_d(2, 2, vals);
    matrix_t *I = test_mat_identity_d(2);

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
    matrix_t *A = test_mat_create_qf(2, 2, vals);
    matrix_t *I = test_mat_identity_qf(2);

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
    matrix_t *A = test_mat_create_qc(2, 2, vals);
    matrix_t *I = test_mat_identity_qc(2);

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
    number_t alpha_num = num_create_from_double(alpha);

    matrix_t *A = test_mat_create_d(2, 2, A_vals);
    print_md("A", A);

    matrix_t *B = mat_scalar_div_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("A / alpha", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_double(A_vals[k] / alpha);
        char label[64];
        snprintf(label, sizeof(label), "scalar div d/d [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_qfloat(alpha);

    matrix_t *A = test_mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_div_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("A / alpha (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_qfloat(qf_div(A_vals[k], alpha));
        char label[64];
        snprintf(label, sizeof(label), "scalar div qf/qf [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_qcomplex(alpha);

    matrix_t *A = test_mat_create_qc(1, 3, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_div_num(A, &alpha_num);
    number_t B_vals[3];
    print_mnum("A / alpha (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t j = 0; j < 3; j++)
    {
        number_t expected_num = num_create_from_qcomplex(qc_div(A_vals[j], alpha));
        number_t error_num = num_abs(num_sub(B_vals[j], expected_num));
        qfloat_t error = num_to_qfloat(error_num);
        char label[64];
        snprintf(label, sizeof(label), "scalar div qc/qc [0,%zu]", j);
        check_bool(label, qf_cmp(error, qf_from_double(1e-28)) <= 0);
        num_destroy(&error_num);
        num_destroy(&expected_num);
        num_destroy(&B_vals[j]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_double(alpha);

    matrix_t *A = test_mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_div_num(A, &alpha_num);
    number_t B_vals[4];
    print_mnum("A / alpha (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        number_t expected = num_create_from_qfloat(
            qf_div(A_vals[k], qf_from_double(alpha)));
        char label[64];
        snprintf(label, sizeof(label), "scalar div d/qf [%zu,%zu]", k / 2, k % 2);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
    number_t alpha_num = num_create_from_qfloat(alpha);

    matrix_t *A = test_mat_create_d(2, 3, A_vals);
    print_md("A (double)", A);

    matrix_t *B = mat_scalar_div_num(A, &alpha_num);
    number_t B_vals[6];
    print_mnum("A / alpha (number)", B);
    mat_get_data_num(B, B_vals);

    for (size_t k = 0; k < 6; k++)
    {
        number_t expected = num_create_from_qfloat(
            qf_div(qf_from_double(A_vals[k]), alpha));
        char label[64];
        snprintf(label, sizeof(label), "scalar div qf/d [%zu,%zu]", k / 3, k % 3);
        check_bool(label, num_eq(B_vals[k], expected));
        num_destroy(&expected);
        num_destroy(&B_vals[k]);
    }

    num_destroy(&alpha_num);
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
        matrix_t *A = test_mat_create_d(1, 1, vals);

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
        matrix_t *A = test_mat_create_d(2, 2, vals);

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
        matrix_t *A = test_mat_create_d(3, 3, vals);

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
        matrix_t *A = test_mat_create_d(2, 2, vals);

        print_md("A (singular)", A);

        double det;
        mat_det(A, &det);
        check_d("det singular = 0", det, 0.0, 1e-30);

        mat_free(A);
    }

    /* -------------------------------------------------------------- identity */
    {
        matrix_t *I = test_mat_identity_d(4);

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

    matrix_t *A = test_mat_create_qf(2, 2, vals);
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

    matrix_t *A = test_mat_create_qc(2, 2, vals);
    print_mqc("A (qcomplex 2x2)", A);

    qcomplex_t det;
    mat_det(A, &det);

    qcomplex_t expected =
        qc_sub(qc_mul(vals[0], vals[3]),
               qc_mul(vals[1], vals[2]));

    check_qc_val("det qcomplex 2x2", det, expected, 1e-28);

    mat_free(A);
}

/* ------------------------------------------------------------------ trace */

static void test_trace(void)
{
    printf(C_CYAN "TEST: trace\n" C_RESET);

    {
        const double vals[9] = {
            1.0, 2.0, 3.0,
            4.0, 5.0, 6.0,
            7.0, 8.0, 9.0};
        matrix_t *A = test_mat_create_d(3, 3, vals);
        double tr = 0.0;

        print_md("A", A);
        check_bool("mat_trace(double) rc = 0", mat_trace(A, &tr) == 0);
        check_d("trace(double) = 15", tr, 15.0, 1e-30);
        mat_free(A);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *xy = dv_mul(x, y);
        dval_t *vals[9] = {
            x,       one,     DV_ZERO,
            DV_ZERO, xy,      two,
            one,     DV_ZERO, y};
        matrix_t *A = mat_create_dv(3, 3, vals);
        dval_t *tr = NULL;

        print_mdv("A (dval)", A);
        check_bool("mat_trace(dval) rc = 0", mat_trace(A, &tr) == 0);
        check_bool("trace(dval) non-null", tr != NULL);
        if (tr) {
            print_det_dv("trace(A)", tr);
            check_d("trace(dval) at x=2,y=3 = 11", dv_eval_d(tr), 11.0, 1e-12);
            check_dval_text_contains("trace(dval) contains x", tr, "x");
            check_dval_text_contains("trace(dval) contains y", tr, "y");
            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);
            check_d("trace(dval) tracks x,y", dv_eval_d(tr), 47.0, 1e-12);
        }

        dv_free(tr);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
        dv_free(xy);
    }
}

static void test_deriv(void)
{
    printf(C_CYAN "TEST: matrix derivative\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *c = test_dv_new_named_const_d(11.0, "c");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *xy = dv_mul(x, y);
        dval_t *x2 = dv_mul(x, x);
        dval_t *sum = dv_add(x2, y);
        dval_t *vals[4] = {
            x,   xy,
            one, sum};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *Dx = mat_deriv(A, x);
        matrix_t *Dy = mat_deriv(A, y);
        matrix_t *Dc = mat_deriv(A, c);
        dval_t *v = NULL;

        print_mdv("A (dval deriv)", A);
        check_bool("mat_deriv(A, x) not NULL", Dx != NULL);
        check_bool("mat_deriv(A, y) not NULL", Dy != NULL);
        check_bool("mat_deriv(A, c) not NULL", Dc != NULL);

        if (Dx) {
            print_mdv("dA/dx", Dx);
            mat_get(Dx, 0, 0, &v);
            check_d("dA/dx[0,0] = 1", dv_eval_d(v), 1.0, 1e-12);
            mat_get(Dx, 0, 1, &v);
            check_d("dA/dx[0,1] = y", dv_eval_d(v), 3.0, 1e-12);
            mat_get(Dx, 1, 0, &v);
            check_d("dA/dx[1,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(Dx, 1, 1, &v);
            check_d("dA/dx[1,1] = 2x", dv_eval_d(v), 4.0, 1e-12);
            mat_get(Dx, 0, 1, &v);
            check_dval_text_contains("dA/dx[0,1] contains y", v, "y");
            mat_get(Dx, 1, 1, &v);
            check_dval_text_contains("dA/dx[1,1] contains x", v, "x");
        }

        if (Dy) {
            print_mdv("dA/dy", Dy);
            mat_get(Dy, 0, 0, &v);
            check_d("dA/dy[0,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(Dy, 0, 1, &v);
            check_d("dA/dy[0,1] = x", dv_eval_d(v), 2.0, 1e-12);
            mat_get(Dy, 1, 0, &v);
            check_d("dA/dy[1,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(Dy, 1, 1, &v);
            check_d("dA/dy[1,1] = 1", dv_eval_d(v), 1.0, 1e-12);
        }

        if (Dc) {
            print_mdv("dA/dc", Dc);
            mat_get(Dc, 0, 0, &v);
            check_bool("dA/dc[0,0] = NaN", v && qf_isnan(dv_eval_qf(v)));
            mat_get(Dc, 0, 1, &v);
            check_bool("dA/dc[0,1] = NaN", v && qf_isnan(dv_eval_qf(v)));
            mat_get(Dc, 1, 0, &v);
            check_bool("dA/dc[1,0] = NaN", v && qf_isnan(dv_eval_qf(v)));
            mat_get(Dc, 1, 1, &v);
            check_bool("dA/dc[1,1] = NaN", v && qf_isnan(dv_eval_qf(v)));
        }

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 7.0);
        if (Dx) {
            mat_get(Dx, 0, 1, &v);
            check_d("dA/dx[0,1] tracks y", dv_eval_d(v), 7.0, 1e-12);
            mat_get(Dx, 1, 1, &v);
            check_d("dA/dx[1,1] tracks x", dv_eval_d(v), 10.0, 1e-12);
        }
        if (Dy) {
            mat_get(Dy, 0, 1, &v);
            check_d("dA/dy[0,1] tracks x", dv_eval_d(v), 5.0, 1e-12);
        }

        mat_free(Dx);
        mat_free(Dy);
        mat_free(Dc);
        mat_free(A);
        dv_free(c);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(xy);
        dv_free(x2);
        dv_free(sum);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        double vals[4] = {
            1.0, 2.0,
            3.0, 4.0};
        double rhs_vals[2] = {5.0, 6.0};
        dval_t *vars[2] = {x, DV_ONE};
        matrix_t *A = test_mat_create_d(2, 2, vals);
        matrix_t *B = test_mat_create_d(2, 1, rhs_vals);
        matrix_t *Expected2 = test_mat_create_d(2, 2, (double[4]){
            0.0, 0.0,
            0.0, 0.0});
        matrix_t *Expected21 = test_mat_create_d(2, 1, (double[2]){
            0.0, 0.0});
        matrix_t *ExpectedJ = mat_create_dv(4, 2, (dval_t *[8]){
            DV_ZERO, DV_ZERO,
            DV_ZERO, DV_ZERO,
            DV_ZERO, DV_ZERO,
            DV_ZERO, DV_ZERO});
        matrix_t *dA = mat_deriv(A, x);
        dval_t *dtr = mat_deriv_trace(A, x);
        dval_t *ddet = mat_deriv_det(A, x);
        matrix_t *dAi = mat_deriv_inverse(A, x);
        matrix_t *dAbi = mat_deriv_block_inverse(A, 1, x);
        matrix_t *dX = mat_deriv_solve(A, B, x);
        matrix_t *dXb = mat_deriv_block_solve(A, B, 1, x);
        matrix_t *J = mat_jacobian(A, vars, 2);

        check_bool("numeric mat_deriv(A, x) not NULL", dA != NULL);
        check_bool("numeric mat_deriv_trace(A, x) not NULL", dtr != NULL);
        check_bool("numeric mat_deriv_det(A, x) not NULL", ddet != NULL);
        check_bool("numeric mat_deriv_inverse(A, x) not NULL", dAi != NULL);
        check_bool("numeric mat_deriv_block_inverse(A, 1, x) not NULL", dAbi != NULL);
        check_bool("numeric mat_deriv_solve(A, B, x) not NULL", dX != NULL);
        check_bool("numeric mat_deriv_block_solve(A, B, 1, x) not NULL", dXb != NULL);
        check_bool("numeric mat_jacobian(A, vars, 2) not NULL", J != NULL);

        if (dA)
            check_mat_d("numeric mat_deriv(A, x) = 0", dA, Expected2, 1e-12);
        if (dtr)
            check_d("numeric mat_deriv_trace(A, x) = 0", dv_eval_d(dtr), 0.0, 1e-12);
        if (ddet)
            check_d("numeric mat_deriv_det(A, x) = 0", dv_eval_d(ddet), 0.0, 1e-12);
        if (dAi)
            check_mat_d("numeric mat_deriv_inverse(A, x) = 0", dAi, Expected2, 1e-12);
        if (dAbi)
            check_mat_d("numeric mat_deriv_block_inverse(A, 1, x) = 0", dAbi, Expected2, 1e-12);
        if (dX)
            check_mat_d("numeric mat_deriv_solve(A, B, x) = 0", dX, Expected21, 1e-12);
        if (dXb)
            check_mat_d("numeric mat_deriv_block_solve(A, B, 1, x) = 0", dXb, Expected21, 1e-12);
        if (J) {
            dval_t *v = NULL;
            for (size_t i = 0; i < 4; ++i) {
                for (size_t j = 0; j < 2; ++j) {
                    char label[64];
                    mat_get(J, i, j, &v);
                    snprintf(label, sizeof(label), "numeric Jacobian[%zu,%zu] = 0", i, j);
                    check_d(label, dv_eval_d(v), 0.0, 1e-12);
                }
            }
            check_bool("numeric Jacobian shape is 4x2",
                       mat_get_row_count(J) == 4 && mat_get_col_count(J) == 2);
            check_bool("numeric Jacobian matches symbolic zero matrix shape",
                       ExpectedJ != NULL &&
                       mat_get_row_count(ExpectedJ) == mat_get_row_count(J) &&
                       mat_get_col_count(ExpectedJ) == mat_get_col_count(J));
        }

        mat_free(J);
        mat_free(dXb);
        mat_free(dX);
        mat_free(dAbi);
        mat_free(dAi);
        dv_free(ddet);
        dv_free(dtr);
        mat_free(dA);
        mat_free(ExpectedJ);
        mat_free(Expected21);
        mat_free(Expected2);
        mat_free(B);
        mat_free(A);
        dv_free(x);
    }
}

static void test_matrix_calculus(void)
{
    printf(C_CYAN "TEST: matrix calculus helpers\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *vals[4] = {
            x,   one,
            y,   two};
        matrix_t *A = mat_create_dv(2, 2, vals);
        dval_t *dtr_dx = mat_deriv_trace(A, x);
        dval_t *dtr_dy = mat_deriv_trace(A, y);
        dval_t *ddet_dx = mat_deriv_det(A, x);
        dval_t *ddet_dy = mat_deriv_det(A, y);
        matrix_t *dAbi_dx = mat_deriv_block_inverse(A, 1, x);
        matrix_t *dAi_dx = mat_deriv_inverse(A, x);
        dval_t *v = NULL;

        print_mdv("A (matrix calculus)", A);
        check_bool("mat_deriv_trace(A, x) not NULL", dtr_dx != NULL);
        check_bool("mat_deriv_trace(A, y) not NULL", dtr_dy != NULL);
        check_bool("mat_deriv_det(A, x) not NULL", ddet_dx != NULL);
        check_bool("mat_deriv_det(A, y) not NULL", ddet_dy != NULL);
        check_bool("mat_deriv_inverse(A, x) not NULL", dAi_dx != NULL);
        check_bool("mat_deriv_block_inverse(A, 1, x) not NULL", dAbi_dx != NULL);

        if (dtr_dx) {
            print_det_dv("d/dx trace(A)", dtr_dx);
            check_d("d/dx trace(A) = 1", dv_eval_d(dtr_dx), 1.0, 1e-12);
        }
        if (dtr_dy) {
            print_det_dv("d/dy trace(A)", dtr_dy);
            check_d("d/dy trace(A) = 0", dv_eval_d(dtr_dy), 0.0, 1e-12);
        }
        if (ddet_dx) {
            print_det_dv("d/dx det(A)", ddet_dx);
            check_d("d/dx det(A) = 2", dv_eval_d(ddet_dx), 2.0, 1e-12);
        }
        if (ddet_dy) {
            print_det_dv("d/dy det(A)", ddet_dy);
            check_d("d/dy det(A) = -1", dv_eval_d(ddet_dy), -1.0, 1e-12);
        }

        if (dAi_dx) {
            print_mdv("d/dx A^{-1} (helper)", dAi_dx);
            mat_get(dAi_dx, 0, 0, &v);
            check_d("d/dx A^{-1}[0,0] = -4", dv_eval_d(v), -4.0, 1e-12);
            mat_get(dAi_dx, 0, 1, &v);
            check_d("d/dx A^{-1}[0,1] = 2", dv_eval_d(v), 2.0, 1e-12);
            mat_get(dAi_dx, 1, 0, &v);
            check_d("d/dx A^{-1}[1,0] = 6", dv_eval_d(v), 6.0, 1e-12);
            mat_get(dAi_dx, 1, 1, &v);
            check_d("d/dx A^{-1}[1,1] = -3", dv_eval_d(v), -3.0, 1e-12);
        }

        if (dAbi_dx) {
            print_mdv("d/dx block_inverse(A, 1) (helper)", dAbi_dx);
            mat_get(dAbi_dx, 0, 0, &v);
            check_d("d/dx block_inverse(A,1)[0,0] = -4", dv_eval_d(v), -4.0, 1e-12);
            mat_get(dAbi_dx, 0, 1, &v);
            check_d("d/dx block_inverse(A,1)[0,1] = 2", dv_eval_d(v), 2.0, 1e-12);
            mat_get(dAbi_dx, 1, 0, &v);
            check_d("d/dx block_inverse(A,1)[1,0] = 6", dv_eval_d(v), 6.0, 1e-12);
            mat_get(dAbi_dx, 1, 1, &v);
            check_d("d/dx block_inverse(A,1)[1,1] = -3", dv_eval_d(v), -3.0, 1e-12);
        }

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 7.0);

        if (ddet_dx)
            check_d("d/dx det(A) tracks updated values", dv_eval_d(ddet_dx), 2.0, 1e-12);
        if (ddet_dy)
            check_d("d/dy det(A) tracks updated values", dv_eval_d(ddet_dy), -1.0, 1e-12);
        if (dAi_dx) {
            mat_get(dAi_dx, 0, 0, &v);
            check_d("d/dx A^{-1}[0,0] tracks updated values", dv_eval_d(v), -4.0 / 9.0, 1e-12);
            mat_get(dAi_dx, 1, 0, &v);
            check_d("d/dx A^{-1}[1,0] tracks updated values", dv_eval_d(v), 14.0 / 9.0, 1e-12);
        }
        if (dAbi_dx) {
            mat_get(dAbi_dx, 0, 0, &v);
            check_d("d/dx block_inverse(A,1)[0,0] tracks updated values", dv_eval_d(v), -4.0 / 9.0, 1e-12);
            mat_get(dAbi_dx, 1, 0, &v);
            check_d("d/dx block_inverse(A,1)[1,0] tracks updated values", dv_eval_d(v), 14.0 / 9.0, 1e-12);
        }

        mat_free(dAbi_dx);
        mat_free(dAi_dx);
        dv_free(ddet_dy);
        dv_free(ddet_dx);
        dv_free(dtr_dy);
        dv_free(dtr_dx);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
    }
}

static void test_deriv_solve(void)
{
    printf(C_CYAN "TEST: derivative of solve\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(4.0, "x");
        dval_t *y = test_dv_new_named_var_d(6.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *zero = DV_ZERO;
        dval_t *A_vals[4] = {
            x,    one,
            zero, two
        };
        dval_t *B_vals[2] = {
            x,
            y
        };
        matrix_t *A = mat_create_dv(2, 2, A_vals);
        matrix_t *B = mat_create_dv(2, 1, B_vals);
        matrix_t *X = mat_solve(A, B);
        matrix_t *dX = mat_deriv_solve(A, B, x);
        matrix_t *dX_expected = NULL;
        matrix_t *dA = NULL;
        matrix_t *dB = NULL;
        matrix_t *AXd = NULL;
        matrix_t *dAX = NULL;
        matrix_t *Residual = NULL;
        dval_t *v = NULL;
        dval_t *w = NULL;

        print_mdv("A (deriv solve)", A);
        print_mdv("B (deriv solve)", B);
        check_bool("mat_solve(A,B) not NULL", X != NULL);
        check_bool("mat_deriv_solve(A,B,x) not NULL", dX != NULL);

        if (X)
            dX_expected = mat_deriv(X, x);

        check_bool("mat_deriv(mat_solve(A,B),x) not NULL", dX_expected != NULL);

        if (dX) {
            print_mdv("d/dx solve(A,B)", dX);
            mat_get(dX, 0, 0, &v);
            check_d("d/dx solve(A,B)[0,0] = 3/16", dv_eval_d(v), 3.0 / 16.0, 1e-12);
            check_dval_text_contains("d/dx solve(A,B)[0,0] contains x", v, "x");
            mat_get(dX, 1, 0, &v);
            check_d("d/dx solve(A,B)[1,0] = 0", dv_eval_d(v), 0.0, 1e-12);
        }

        if (dX && dX_expected) {
            for (size_t i = 0; i < 2; ++i) {
                char label[64];
                mat_get(dX, i, 0, &v);
                mat_get(dX_expected, i, 0, &w);
                snprintf(label, sizeof(label), "d/dx solve(A,B)[%zu,0] matches direct derivative", i);
                check_d(label, dv_eval_d(v), dv_eval_d(w), 1e-12);
            }
        }

        if (X && dX) {
            dA = mat_deriv(A, x);
            dB = mat_deriv(B, x);
            AXd = mat_mul(A, dX);
            dAX = dA ? mat_mul(dA, X) : NULL;
            if (AXd && dAX)
                Residual = mat_add(AXd, dAX);
            if (Residual && dB) {
                matrix_t *Tmp = mat_sub(Residual, dB);
                mat_free(Residual);
                Residual = Tmp;
            }

            check_bool("A*dX + dA*X - dB not NULL", Residual != NULL);
            if (Residual) {
                print_mdv("A*dX + dA*X - dB", Residual);
                for (size_t i = 0; i < 2; ++i) {
                    char label[64];
                    mat_get(Residual, i, 0, &v);
                    snprintf(label, sizeof(label), "solve derivative residual[%zu,0]", i);
                    check_d(label, dv_eval_d(v), 0.0, 1e-12);
                }
            }
        }

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 8.0);
        if (dX && dX_expected) {
            mat_get(dX, 0, 0, &v);
            mat_get(dX_expected, 0, 0, &w);
            check_d("d/dx solve(A,B)[0,0] tracks updates", dv_eval_d(v), dv_eval_d(w), 1e-12);
            mat_get(dX, 1, 0, &v);
            check_d("d/dx solve(A,B)[1,0] stays zero after updates", dv_eval_d(v), 0.0, 1e-12);
        }

        mat_free(Residual);
        mat_free(dAX);
        mat_free(AXd);
        mat_free(dB);
        mat_free(dA);
        mat_free(dX_expected);
        mat_free(dX);
        mat_free(X);
        mat_free(B);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
    }
}

static void test_deriv_block_solve(void)
{
    printf(C_CYAN "TEST: derivative of block solve\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *five = test_dv_new_const_d(5.0);
        dval_t *vals[9] = {
            x,     one,  two,
            three, y,    four,
            one,   two,  five};
        dval_t *rhs_vals[3] = { x, two, one };
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *B = mat_create_dv(3, 1, rhs_vals);
        matrix_t *X = NULL;
        matrix_t *dX = NULL;
        matrix_t *dA = NULL;
        matrix_t *dB = NULL;
        matrix_t *AXd = NULL;
        matrix_t *dAX = NULL;
        matrix_t *Residual = NULL;
        matrix_t *X_for_residual = NULL;
        dval_t *v = NULL;

        X = mat_block_solve(A, B, 1);
        dX = mat_deriv_block_solve(A, B, 1, x);

        print_mdv("A (dval block deriv)", A);
        print_mdv("B (dval block deriv)", B);
        check_bool("mat_block_solve(A,B,1) not NULL", X != NULL);
        check_bool("mat_deriv_block_solve(A,B,1,x) not NULL", dX != NULL);
        if (dX) {
            print_mdv("d/dx block_solve(A,B)", dX);

            mat_get(dX, 0, 0, &v);
            check_d("d(block solve)[0,0] = -7/81", dv_eval_d(v), -0.08641975308641975, 1e-12);
            check_dval_text_contains("d(block solve)[0,0] contains x", v, "x");
            mat_get(dX, 1, 0, &v);
            check_d("d(block solve)[1,0] = 11/81", dv_eval_d(v), 0.1358024691358025, 1e-12);
            mat_get(dX, 2, 0, &v);
            check_d("d(block solve)[2,0] = -1/27", dv_eval_d(v), -0.03703703703703703, 1e-12);
        }

        dA = mat_deriv(A, x);
        dB = mat_deriv(B, x);
        AXd = mat_mul(A, dX);
        X_for_residual = mat_block_solve(A, B, 1);
        dAX = mat_mul(dA, X_for_residual);
        if (AXd && dAX) {
            Residual = mat_add(AXd, dAX);
            if (Residual) {
                matrix_t *Tmp = mat_sub(Residual, dB);
                mat_free(Residual);
                Residual = Tmp;
            }
        }

        check_bool("A*dX + dA*X - dB not NULL", Residual != NULL);
        if (Residual) {
            mat_get(Residual, 0, 0, &v);
            check_d("block solve derivative residual[0,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(Residual, 1, 0, &v);
            check_d("block solve derivative residual[1,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(Residual, 2, 0, &v);
            check_d("block solve derivative residual[2,0] = 0", dv_eval_d(v), 0.0, 1e-12);
        }

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 7.0);
        if (dX) {
            mat_get(dX, 0, 0, &v);
            check_d("updated d(block solve)[0,0] = -0.001814028486965869", dv_eval_d(v), -0.001814028486965869, 1e-12);
            mat_get(dX, 1, 0, &v);
            check_d("updated d(block solve)[1,0] = 0.0007390486428379468", dv_eval_d(v), 0.0007390486428379468, 1e-12);
            mat_get(dX, 2, 0, &v);
            check_d("updated d(block solve)[2,0] = 6.718624025799516e-05", dv_eval_d(v), 6.718624025799516e-05, 1e-12);
        }

        mat_free(Residual);
        mat_free(dAX);
        mat_free(AXd);
        mat_free(dB);
        mat_free(dA);
        mat_free(dX);
        mat_free(X_for_residual);
        mat_free(B);
        mat_free(X);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
        dv_free(three);
        dv_free(four);
        dv_free(five);
    }
}

static void test_jacobian(void)
{
    printf(C_CYAN "TEST: matrix Jacobian\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *xy = dv_mul(x, y);
        dval_t *x2 = dv_mul(x, x);
        dval_t *sum = dv_add(x2, y);
        dval_t *vals[4] = {
            x,   xy,
            DV_ONE, sum
        };
        dval_t *vars[2] = {x, y};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *J = mat_jacobian(A, vars, 2);
        dval_t *v = NULL;

        print_mdv("A (Jacobian)", A);
        check_bool("mat_jacobian(A, [x,y]) not NULL", J != NULL);
        check_bool("Jacobian rows = rows*cols",
                   J != NULL && mat_get_row_count(J) == 4);
        check_bool("Jacobian cols = nvars",
                   J != NULL && mat_get_col_count(J) == 2);

        if (J) {
            print_mdv("J(A; x,y)", J);

            mat_get(J, 0, 0, &v);
            check_d("J[0,0] = dA[0,0]/dx = 1", dv_eval_d(v), 1.0, 1e-12);
            mat_get(J, 0, 1, &v);
            check_d("J[0,1] = dA[0,0]/dy = 0", dv_eval_d(v), 0.0, 1e-12);

            mat_get(J, 1, 0, &v);
            check_d("J[1,0] = dA[0,1]/dx = y", dv_eval_d(v), 3.0, 1e-12);
            check_dval_text_contains("J[1,0] contains y", v, "y");
            mat_get(J, 1, 1, &v);
            check_d("J[1,1] = dA[0,1]/dy = x", dv_eval_d(v), 2.0, 1e-12);
            check_dval_text_contains("J[1,1] contains x", v, "x");

            mat_get(J, 2, 0, &v);
            check_d("J[2,0] = dA[1,0]/dx = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(J, 2, 1, &v);
            check_d("J[2,1] = dA[1,0]/dy = 0", dv_eval_d(v), 0.0, 1e-12);

            mat_get(J, 3, 0, &v);
            check_d("J[3,0] = dA[1,1]/dx = 2x", dv_eval_d(v), 4.0, 1e-12);
            check_dval_text_contains("J[3,0] contains x", v, "x");
            mat_get(J, 3, 1, &v);
            check_d("J[3,1] = dA[1,1]/dy = 1", dv_eval_d(v), 1.0, 1e-12);

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);

            mat_get(J, 1, 0, &v);
            check_d("J[1,0] tracks updated y", dv_eval_d(v), 7.0, 1e-12);
            mat_get(J, 1, 1, &v);
            check_d("J[1,1] tracks updated x", dv_eval_d(v), 5.0, 1e-12);
            mat_get(J, 3, 0, &v);
            check_d("J[3,0] tracks updated x", dv_eval_d(v), 10.0, 1e-12);
        }

        mat_free(J);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(xy);
        dv_free(x2);
        dv_free(sum);
    }
}

static void test_schur_complement(void)
{
    printf(C_CYAN "TEST: Schur complement\n" C_RESET);

    {
        double vals[9] = {
            2.0, 1.0, 3.0,
            4.0, 5.0, 6.0,
            7.0, 8.0, 10.0};
        matrix_t *A = test_mat_create_d(3, 3, vals);
        matrix_t *S = mat_schur_complement(A, 1);
        double s00 = 0.0, s01 = 0.0, s10 = 0.0, s11 = 0.0;
        double detA = 0.0, detS = 0.0;

        print_md("A (double Schur complement)", A);
        check_bool("mat_schur_complement(double) not NULL", S != NULL);
        if (S) {
            print_md("S = A22 - A21 A11^{-1} A12", S);
            check_bool("Schur complement(double) rows = 2", mat_get_row_count(S) == 2);
            check_bool("Schur complement(double) cols = 2", mat_get_col_count(S) == 2);
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("S[0,0] = 3", s00, 3.0, 1e-12);
            check_d("S[0,1] = 0", s01, 0.0, 1e-12);
            check_d("S[1,0] = 4.5", s10, 4.5, 1e-12);
            check_d("S[1,1] = -0.5", s11, -0.5, 1e-12);

            check_bool("mat_det(A) rc = 0", mat_det(A, &detA) == 0);
            check_bool("mat_det(S) rc = 0", mat_det(S, &detS) == 0);
            check_d("det(A) = det(A11)*det(S)", detA, 2.0 * detS, 1e-12);
        }

        mat_free(S);
        mat_free(A);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *five = test_dv_new_const_d(5.0);
        dval_t *vals[9] = {
            x,     one,  two,
            three, y,    four,
            one,   two,  five};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *S = mat_schur_complement(A, 1);
        dval_t *s00 = NULL, *s01 = NULL, *s10 = NULL, *s11 = NULL;
        dval_t *detA = NULL, *detS = NULL;
        dval_t *lhs = NULL, *rhs = NULL;
        dval_t *raw = NULL;

        print_mdv("A (dval Schur complement)", A);
        check_bool("mat_schur_complement(dval) not NULL", S != NULL);
        if (S) {
            print_mdv("S = A22 - A21 A11^{-1} A12", S);
            check_bool("Schur complement(dval) rows = 2", mat_get_row_count(S) == 2);
            check_bool("Schur complement(dval) cols = 2", mat_get_col_count(S) == 2);

            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("S(dval)[0,0] at x=2,y=3", dv_eval_d(s00), 1.5, 1e-12);
            check_d("S(dval)[0,1] at x=2", dv_eval_d(s01), 1.0, 1e-12);
            check_d("S(dval)[1,0] at x=2", dv_eval_d(s10), 1.5, 1e-12);
            check_d("S(dval)[1,1] at x=2", dv_eval_d(s11), 4.0, 1e-12);
            check_dval_text_contains("S(dval)[0,0] contains x", s00, "x");
            check_dval_text_contains("S(dval)[0,0] contains y", s00, "y");

            check_bool("mat_det(dval A) rc = 0", mat_det(A, &detA) == 0);
            check_bool("mat_det(dval S) rc = 0", mat_det(S, &detS) == 0);
            check_bool("det(A) not NULL", detA != NULL);
            check_bool("det(S) not NULL", detS != NULL);
            if (detA && detS) {
                raw = dv_mul(x, detS);
                lhs = dv_simplify(raw);
                dv_free(raw);
                raw = NULL;

                rhs = dv_simplify(detA);

                check_bool("det identity lhs not NULL", lhs != NULL);
                check_bool("det identity rhs not NULL", rhs != NULL);
                if (lhs && rhs)
                    check_d("det(A) = x*det(S)", dv_eval_d(lhs), dv_eval_d(rhs), 1e-12);
            }

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);
            check_d("S(dval)[0,0] tracks x,y", dv_eval_d(s00), 6.4, 1e-12);
            check_d("S(dval)[0,1] tracks x", dv_eval_d(s01), 2.8, 1e-12);
            check_d("S(dval)[1,0] tracks x", dv_eval_d(s10), 1.8, 1e-12);
            check_d("S(dval)[1,1] tracks x", dv_eval_d(s11), 4.6, 1e-12);
        }

        dv_free(raw);
        dv_free(lhs);
        dv_free(rhs);
        dv_free(detA);
        dv_free(detS);
        mat_free(S);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
        dv_free(three);
        dv_free(four);
        dv_free(five);
    }
}

static void test_block_linear_algebra(void)
{
    printf(C_CYAN "TEST: block inverse and block solve\n" C_RESET);

    {
        double vals[9] = {
            2.0, 1.0, 3.0,
            4.0, 5.0, 6.0,
            7.0, 8.0, 10.0};
        double xvals[6] = {
            1.0, 2.0,
            3.0, 4.0,
            5.0, 6.0};
        matrix_t *A = test_mat_create_d(3, 3, vals);
        matrix_t *Xexp = test_mat_create_d(3, 2, xvals);
        matrix_t *B = mat_mul(A, Xexp);
        matrix_t *Ainv = mat_block_inverse(A, 1);
        matrix_t *I = mat_mul(A, Ainv);
        matrix_t *X = mat_block_solve(A, B, 1);
        matrix_t *AX = mat_mul(A, X);
        double got = 0.0;

        print_md("A (double block)", A);
        check_bool("mat_block_inverse(double) not NULL", Ainv != NULL);
        check_bool("A * block_inverse(A) not NULL", I != NULL);
        if (I) {
            print_md("A * block_inverse(A)", I);
            mat_get(I, 0, 0, &got);
            check_d("block inverse prod[0,0] = 1", got, 1.0, 1e-12);
            mat_get(I, 0, 1, &got);
            check_d("block inverse prod[0,1] = 0", got, 0.0, 1e-12);
            mat_get(I, 2, 2, &got);
            check_d("block inverse prod[2,2] = 1", got, 1.0, 1e-12);
        }

        check_bool("mat_block_solve(double) not NULL", X != NULL);
        check_bool("A * block_solve(A,B) not NULL", AX != NULL);
        if (X) {
            print_md("block solve X (double)", X);
            mat_get(X, 0, 0, &got);
            check_d("block solve X[0,0] = 1", got, 1.0, 1e-12);
            mat_get(X, 2, 1, &got);
            check_d("block solve X[2,1] = 6", got, 6.0, 1e-12);
        }
        if (AX) {
            mat_get(AX, 1, 0, &got);
            check_d("block solve residual[1,0]", got, 49.0, 1e-12);
            mat_get(AX, 2, 1, &got);
            check_d("block solve residual[2,1]", got, 106.0, 1e-12);
        }

        mat_free(AX);
        mat_free(X);
        mat_free(I);
        mat_free(Ainv);
        mat_free(B);
        mat_free(Xexp);
        mat_free(A);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *u = test_dv_new_named_var_d(5.0, "u");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *five = test_dv_new_const_d(5.0);
        dval_t *xvals[9] = {
            x,     one,  two,
            three, y,    four,
            one,   two,  five};
        dval_t *rhs_vals[3] = { u, two, one };
        matrix_t *A = mat_create_dv(3, 3, xvals);
        matrix_t *Xexp = mat_create_dv(3, 1, rhs_vals);
        matrix_t *B = mat_mul(A, Xexp);
        matrix_t *Ainv = mat_block_inverse(A, 1);
        matrix_t *I = mat_mul(A, Ainv);
        matrix_t *X = mat_block_solve(A, B, 1);
        matrix_t *AX = mat_mul(A, X);
        dval_t *v = NULL;

        print_mdv("A (dval block)", A);
        check_bool("mat_block_inverse(dval) not NULL", Ainv != NULL);
        check_bool("A * block_inverse(A) dval not NULL", I != NULL);
        if (I) {
            print_mdv("A * block_inverse(A) (dval)", I);
            mat_get(I, 0, 0, &v);
            check_d("dval block inverse prod[0,0] = 1", dv_eval_d(v), 1.0, 1e-12);
            mat_get(I, 1, 2, &v);
            check_d("dval block inverse prod[1,2] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(I, 2, 2, &v);
            check_d("dval block inverse prod[2,2] = 1", dv_eval_d(v), 1.0, 1e-12);
        }

        check_bool("mat_block_solve(dval) not NULL", X != NULL);
        check_bool("A * block_solve(A,B) dval not NULL", AX != NULL);
        if (X) {
            print_mdv("block solve X (dval)", X);
            mat_get(X, 0, 0, &v);
            check_d("dval block solve X[0,0] = u", dv_eval_d(v), 5.0, 1e-12);
            mat_get(X, 1, 0, &v);
            check_d("dval block solve X[1,0] = 2", dv_eval_d(v), 2.0, 1e-12);
            mat_get(X, 2, 0, &v);
            check_d("dval block solve X[2,0] = 1", dv_eval_d(v), 1.0, 1e-12);

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);
            test_dv_set_val_d(u, 11.0);
            mat_get(X, 0, 0, &v);
            check_d("dval block solve X[0,0] tracks u", dv_eval_d(v), 11.0, 1e-12);
            mat_get(X, 1, 0, &v);
            check_d("dval block solve X[1,0] remains 2", dv_eval_d(v), 2.0, 1e-12);
            mat_get(X, 2, 0, &v);
            check_d("dval block solve X[2,0] remains 1", dv_eval_d(v), 1.0, 1e-12);
        }
        if (AX) {
            mat_get(AX, 0, 0, &v);
            check_d("dval block solve residual[0,0]", dv_eval_d(v), 59.0, 1e-12);
            mat_get(AX, 1, 0, &v);
            check_d("dval block solve residual[1,0]", dv_eval_d(v), 51.0, 1e-12);
            mat_get(AX, 2, 0, &v);
            check_d("dval block solve residual[2,0]", dv_eval_d(v), 20.0, 1e-12);
        }

        mat_free(AX);
        mat_free(X);
        mat_free(I);
        mat_free(Ainv);
        mat_free(B);
        mat_free(Xexp);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(u);
        dv_free(one);
        dv_free(two);
        dv_free(three);
        dv_free(four);
        dv_free(five);
    }
}

static void test_evaluate_bridge(void)
{
    printf(C_CYAN "TEST: symbolic evaluation bridge\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *xy = dv_mul(x, y);
        dval_t *vals[4] = {
            x,  one,
            xy, y
        };
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *N = mat_evaluate_num(A);
        number_t got;
        number_t two = num_create_from_long(2);
        number_t six = num_create_from_long(6);

        print_mdv("A (dval)", A);
        check_bool("mat_evaluate_num(dval) not NULL", N != NULL);
        check_bool("mat_evaluate_num(dval) -> MAT_TYPE_NUMBER",
                   N != NULL && mat_typeof(N) == MAT_TYPE_NUMBER);
        if (N) {
            print_mnum("evaluate_num(A)", N);

            got = mat_get_num(N, 0, 0);
            check_bool("evaluate_num(A)[0,0] = x", num_eq(got, two));
            num_destroy(&got);
            got = mat_get_num(N, 1, 0);
            check_bool("evaluate_num(A)[1,0] = x*y", num_eq(got, six));
            num_destroy(&got);

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);

            got = mat_get_num(N, 0, 0);
            check_bool("evaluate_num(A) snapshot stays at old x", num_eq(got, two));
            num_destroy(&got);
            got = mat_get_num(N, 1, 0);
            check_bool("evaluate_num(A) snapshot stays at old x*y", num_eq(got, six));
            num_destroy(&got);
        }

        num_destroy(&six);
        num_destroy(&two);
        mat_free(N);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(xy);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *xy = dv_mul(x, y);
        number_t two = num_create_from_long(2);
        number_t six = num_create_from_long(6);
        dval_t *vals[4] = {
            x,  one,
            xy, y
        };
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *N = mat_evaluate_num(A);
        number_t got;

        check_bool("mat_evaluate_num(dval) not NULL", N != NULL);
        check_bool("mat_evaluate_num(dval) -> MAT_TYPE_NUMBER",
                   N != NULL && mat_typeof(N) == MAT_TYPE_NUMBER);
        if (N) {
            got = mat_get_num(N, 0, 0);
            check_bool("mat_evaluate_num(dval)[0,0] = x", num_eq(got, two));
            num_destroy(&got);

            got = mat_get_num(N, 1, 0);
            check_bool("mat_evaluate_num(dval)[1,0] = x*y", num_eq(got, six));
            num_destroy(&got);

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);

            got = mat_get_num(N, 0, 0);
            check_bool("mat_evaluate_num(dval) snapshot stays at old x", num_eq(got, two));
            num_destroy(&got);

            got = mat_get_num(N, 1, 0);
            check_bool("mat_evaluate_num(dval) snapshot stays at old x*y", num_eq(got, six));
            num_destroy(&got);
        }

        mat_free(N);
        mat_free(A);
        num_destroy(&six);
        num_destroy(&two);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(xy);
    }

    {
        dval_t *a = test_dv_new_named_var_d(4.0, "a");
        dval_t *b = test_dv_new_named_var_d(5.0, "b");
        dval_t *vals[4] = {
            a, DV_ZERO,
            DV_ONE, b
        };
        matrix_t *T = mat_create_dv(2, 2, vals);
        matrix_t *N = mat_evaluate_num(T);
        number_t got;
        number_t four = num_create_from_long(4);
        number_t five = num_create_from_long(5);
        number_t one_num = num_create_from_long(1);

        print_mdv("T (dval triangular)", T);
        check_bool("mat_evaluate_num(dval triangular) not NULL", N != NULL);
        check_bool("mat_evaluate_num(dval triangular) -> MAT_TYPE_NUMBER",
                   N != NULL && mat_typeof(N) == MAT_TYPE_NUMBER);
        check_bool("mat_evaluate_num preserves lower-triangular structure",
                   N != NULL && mat_is_lower_triangular(N));
        if (N) {
            print_mnum("evaluate_num(T)", N);

            got = mat_get_num(N, 0, 0);
            check_bool("evaluate_num(T)[0,0] = a", num_eq(got, four));
            num_destroy(&got);
            got = mat_get_num(N, 1, 0);
            check_bool("evaluate_num(T)[1,0] = 1", num_eq(got, one_num));
            num_destroy(&got);

            test_dv_set_val_d(a, 9.0);
            test_dv_set_val_d(b, 11.0);

            got = mat_get_num(N, 0, 0);
            check_bool("evaluate_num(T) snapshot stays at old a", num_eq(got, four));
            num_destroy(&got);
            got = mat_get_num(N, 1, 1);
            check_bool("evaluate_num(T) snapshot stays at old b", num_eq(got, five));
            num_destroy(&got);
        }

        num_destroy(&one_num);
        num_destroy(&five);
        num_destroy(&four);
        mat_free(N);
        mat_free(T);
        dv_free(a);
        dv_free(b);
    }

    {
        number_t vals[4] = {
            num_create_from_long(1),
            num_create_from_long(2),
            num_create_from_string("3.125"),
            num_create_from_mfloat_with_prec_bits(MF_PI, 512)
        };
        matrix_t *A = mat_create_num(2, 2, vals);
        matrix_t *N = mat_evaluate_num(A);
        number_t got;

        check_bool("mat_evaluate_num(number) not NULL", N != NULL);
        check_bool("mat_evaluate_num(number) -> MAT_TYPE_NUMBER",
                   N != NULL && mat_typeof(N) == MAT_TYPE_NUMBER);
        if (N) {
            got = mat_get_num(N, 1, 1);
            check_bool("mat_evaluate_num(number) preserves multiprecision value",
                       num_eq(got, vals[3]));
            check_bool("mat_evaluate_num(number) preserves precision bits",
                       num_get_prec_bits(got) == 512);
            num_destroy(&got);

            got = mat_get_num(N, 0, 1);
            check_bool("mat_evaluate_num(number) copies ordinary entries",
                       num_eq(got, vals[1]));
            num_destroy(&got);
        }

        mat_free(N);
        mat_free(A);
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&vals[i]);
    }
}

/* ------------------------------------------------------------------ inverse: double */

static void test_inverse_double(void)
{
    printf(C_CYAN "TEST: matrix inverse (double)\n" C_RESET);

    /* 2×2 invertible */
    {
        double A_vals[4] = {4, 7, 2, 6};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);

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
        matrix_t *I = test_mat_identity_d(3);
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);

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
    matrix_t *A = test_mat_create_qf(2, 2, A_vals);

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
    matrix_t *A = test_mat_create_qc(2, 2, A_vals);

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

static void test_inverse_dval(void)
{
    printf(C_CYAN "TEST: matrix inverse (dval)\n" C_RESET);

    dval_t *x = test_dv_new_named_var_d(3.0, "x");
    dval_t *y = test_dv_new_named_var_d(4.0, "y");
    dval_t *one = test_dv_new_const_d(1.0);
    dval_t *two = test_dv_new_const_d(2.0);
    dval_t *vals[4] = {x, one, y, two};
    matrix_t *A = mat_create_dv(2, 2, vals);
    matrix_t *Ai = mat_inverse(A);
    matrix_t *P = NULL;
    dval_t *v = NULL;

    print_mdv("A", A);
    check_bool("inverse(dval 2x2) returned non-null", Ai != NULL);

    if (Ai) {
        char *ai_text = mat_to_string(Ai, MAT_STRING_INLINE_PRETTY);

        print_mdv("A^{-1}", Ai);
        check_bool("inverse(dval 2x2) exact text simplified",
                   ai_text && strcmp(ai_text,
                                     "{ (2/(2x - y), -1/(2x - y); -y/(2x - y), x/(2x - y)) | x = 3, y = 4 }") == 0);
        P = mat_mul(A, Ai);
        check_bool("A * A^{-1} (dval) non-null", P != NULL);
        if (P) {
            print_mdv("A * A^{-1}", P);

            mat_get(P, 0, 0, &v);
            check_d("dval prod[0,0] = 1", dv_eval_d(v), 1.0, 1e-12);
            mat_get(P, 0, 1, &v);
            check_d("dval prod[0,1] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(P, 1, 0, &v);
            check_d("dval prod[1,0] = 0", dv_eval_d(v), 0.0, 1e-12);
            mat_get(P, 1, 1, &v);
            check_d("dval prod[1,1] = 1", dv_eval_d(v), 1.0, 1e-12);

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 6.0);
            mat_get(P, 0, 0, &v);
            check_d("dval inverse product tracks x,y on [0,0]", dv_eval_d(v), 1.0, 1e-12);
            mat_get(P, 1, 1, &v);
            check_d("dval inverse product tracks x,y on [1,1]", dv_eval_d(v), 1.0, 1e-12);
        }

        free(ai_text);
    }

    mat_free(P);
    mat_free(Ai);
    mat_free(A);
    dv_free(x);
    dv_free(y);
    dv_free(one);
    dv_free(two);

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *R = mat_from_string("(cos(@theta), -sin(@theta); sin(@theta), cos(@theta))",
                                      &bindings);
        matrix_t *Ri = mat_inverse(R);
        char *ri_text = Ri ? mat_to_string(Ri, MAT_STRING_INLINE_PRETTY) : NULL;

        check_bool("inverse(rotation matrix) returned non-null", Ri != NULL);
        check_bool("inverse(rotation matrix) symbolic text contains cos(θ)",
                   ri_text && strstr(ri_text, "cos(θ)") != NULL);
        check_bool("inverse(rotation matrix) symbolic text contains sin(θ)",
                   ri_text && strstr(ri_text, "sin(θ)") != NULL);
        check_bool("inverse(rotation matrix) symbolic text contains -sin(θ)",
                   ri_text && strstr(ri_text, "-sin(θ)") != NULL);

        free(ri_text);
        mat_free(Ri);
        mat_free(R);
        mat_bindings_free(bindings);
    }

    {
        dval_t *a = test_dv_new_named_var_d(2.0, "a");
        dval_t *b = test_dv_new_named_var_d(3.0, "b");
        dval_t *c = test_dv_new_named_var_d(5.0, "c");
        dval_t *d = test_dv_new_named_var_d(7.0, "d");
        dval_t *one_u = test_dv_new_const_d(1.0);
        dval_t *zero = DV_ZERO;
        dval_t *vals[16] = {
            a, b, c, one_u,
            zero, d, one_u, c,
            zero, zero, a, b,
            zero, zero, zero, d
        };
        matrix_t *U = mat_create_dv(4, 4, vals);
        matrix_t *Ui = mat_inverse(U);
        matrix_t *P = NULL;
        dval_t *v = NULL;

        check_bool("inverse(upper triangular dval) returned non-null", Ui != NULL);

        if (Ui) {
            P = mat_mul(U, Ui);
            check_bool("U * U^{-1} non-null", P != NULL);
            if (P) {
                for (size_t i = 0; i < 4; ++i) {
                    for (size_t j = 0; j < 4; ++j) {
                        char label[64];
                        mat_get(P, i, j, &v);
                        snprintf(label, sizeof(label), "upper dval prod[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(v), i == j ? 1.0 : 0.0, 1e-12);
                    }
                }
            }
        }

        mat_free(P);
        mat_free(Ui);
        mat_free(U);
        dv_free(a);
        dv_free(b);
        dv_free(c);
        dv_free(d);
        dv_free(one_u);
    }

    {
        dval_t *p = test_dv_new_named_var_d(2.0, "p");
        dval_t *q = test_dv_new_named_var_d(4.0, "q");
        dval_t *r = test_dv_new_named_var_d(6.0, "r");
        dval_t *one_l = test_dv_new_const_d(1.0);
        dval_t *two_l = test_dv_new_const_d(2.0);
        dval_t *vals[9] = {
            p,      DV_ZERO, DV_ZERO,
            one_l,  q,       DV_ZERO,
            two_l,  one_l,   r
        };
        matrix_t *L = mat_create_dv(3, 3, vals);
        matrix_t *Li = mat_inverse(L);
        matrix_t *P = NULL;
        dval_t *v = NULL;

        check_bool("inverse(lower triangular dval) returned non-null", Li != NULL);

        if (Li) {
            P = mat_mul(L, Li);
            check_bool("L * L^{-1} non-null", P != NULL);
            if (P) {
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        char label[64];
                        mat_get(P, i, j, &v);
                        snprintf(label, sizeof(label), "lower dval prod[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(v), i == j ? 1.0 : 0.0, 1e-12);
                    }
                }
            }
        }

        mat_free(P);
        mat_free(Li);
        mat_free(L);
        dv_free(p);
        dv_free(q);
        dv_free(r);
        dv_free(one_l);
        dv_free(two_l);
    }

    {
        dval_t *x3 = test_dv_new_named_var_d(4.0, "x");
        dval_t *y3 = test_dv_new_named_var_d(3.0, "y");
        dval_t *z3 = test_dv_new_named_var_d(5.0, "z");
        dval_t *one3 = test_dv_new_const_d(1.0);
        dval_t *two3 = test_dv_new_const_d(2.0);
        dval_t *vals[9] = {
            x3,   one3, two3,
            one3, y3,   z3,
            two3, one3, x3
        };
        matrix_t *A3 = mat_create_dv(3, 3, vals);
        matrix_t *A3i = mat_inverse(A3);
        matrix_t *P = NULL;
        dval_t *v = NULL;

        check_bool("inverse(dense 3x3 dval) returned non-null", A3i != NULL);

        if (A3i) {
            P = mat_mul(A3, A3i);
            check_bool("A * A^{-1} (3x3 dval) non-null", P != NULL);
            if (P) {
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        char label[64];
                        mat_get(P, i, j, &v);
                        snprintf(label, sizeof(label), "dense 3x3 dval prod[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(v), i == j ? 1.0 : 0.0, 1e-10);
                    }
                }
            }
        }

        mat_free(P);
        mat_free(A3i);
        mat_free(A3);
        dv_free(x3);
        dv_free(y3);
        dv_free(z3);
        dv_free(one3);
        dv_free(two3);
    }

    {
        dval_t *u = test_dv_new_named_var_d(5.0, "u");
        dval_t *v4 = test_dv_new_named_var_d(6.0, "v");
        dval_t *w = test_dv_new_named_var_d(7.0, "w");
        dval_t *t = test_dv_new_named_var_d(8.0, "t");
        dval_t *one4 = test_dv_new_const_d(1.0);
        dval_t *two4 = test_dv_new_const_d(2.0);
        dval_t *zero4 = DV_ZERO;
        dval_t *vals[16] = {
            u,    one4, zero4, two4,
            one4, v4,   one4,  zero4,
            zero4, one4, w,    one4,
            two4, zero4, one4, t
        };
        matrix_t *A4 = mat_create_dv(4, 4, vals);
        matrix_t *A4i = mat_inverse(A4);
        matrix_t *P = NULL;
        dval_t *entry = NULL;

        check_bool("inverse(dense 4x4 dval) returned non-null", A4i != NULL);

        if (A4i) {
            P = mat_mul(A4, A4i);
            check_bool("A * A^{-1} (4x4 dval) non-null", P != NULL);
            if (P) {
                for (size_t i = 0; i < 4; ++i) {
                    for (size_t j = 0; j < 4; ++j) {
                        char label[64];
                        mat_get(P, i, j, &entry);
                        snprintf(label, sizeof(label), "dense 4x4 dval prod[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(entry), i == j ? 1.0 : 0.0, 1e-10);
                    }
                }
            }
        }

        mat_free(P);
        mat_free(A4i);
        mat_free(A4);
        dv_free(u);
        dv_free(v4);
        dv_free(w);
        dv_free(t);
        dv_free(one4);
        dv_free(two4);
    }

    {
        dval_t *a6 = test_dv_new_named_var_d(5.0, "a");
        dval_t *b6 = test_dv_new_named_var_d(6.0, "b");
        dval_t *c6 = test_dv_new_named_var_d(7.0, "c");
        dval_t *d6 = test_dv_new_named_var_d(8.0, "d");
        dval_t *e6 = test_dv_new_named_var_d(9.0, "e");
        dval_t *f6 = test_dv_new_named_var_d(10.0, "f");
        dval_t *one6 = test_dv_new_const_d(1.0);
        dval_t *two6 = test_dv_new_const_d(2.0);
        dval_t *zero6 = DV_ZERO;
        dval_t *vals[36] = {
            a6,   one6, two6, zero6, zero6, zero6,
            one6, b6,   one6, zero6, zero6, zero6,
            two6, one6, c6,   one6, zero6, zero6,
            zero6, zero6, one6, d6,   one6, two6,
            zero6, zero6, zero6, one6, e6,   one6,
            zero6, zero6, zero6, two6, one6, f6
        };
        matrix_t *A6 = mat_create_dv(6, 6, vals);
        matrix_t *A6i = mat_inverse(A6);
        matrix_t *P = NULL;
        dval_t *entry = NULL;

        check_bool("inverse(dense 6x6 dval) returned non-null", A6i != NULL);

        if (A6i) {
            P = mat_mul(A6, A6i);
            check_bool("A * A^{-1} (6x6 dval) non-null", P != NULL);
            if (P) {
                for (size_t i = 0; i < 6; ++i) {
                    for (size_t j = 0; j < 6; ++j) {
                        char label[64];
                        mat_get(P, i, j, &entry);
                        snprintf(label, sizeof(label), "dense 6x6 dval prod[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(entry), i == j ? 1.0 : 0.0, 1e-10);
                    }
                }
            }
        }

        mat_free(P);
        mat_free(A6i);
        mat_free(A6);
        dv_free(a6);
        dv_free(b6);
        dv_free(c6);
        dv_free(d6);
        dv_free(e6);
        dv_free(f6);
        dv_free(one6);
        dv_free(two6);
    }

    {
        dval_t *one_s = test_dv_new_const_d(1.0);
        dval_t *two_s = test_dv_new_const_d(2.0);
        dval_t *four_s = test_dv_new_const_d(4.0);
        dval_t *sing_vals[4] = {one_s, two_s, two_s, four_s};
        matrix_t *S = mat_create_dv(2, 2, sing_vals);
        matrix_t *Si = mat_inverse(S);

        print_mdv("A (singular dval)", S);
        check_bool("inverse(singular dval) = NULL", Si == NULL);

        mat_free(Si);
        mat_free(S);
        dv_free(one_s);
        dv_free(two_s);
        dv_free(four_s);
    }
}

static void test_det_dval(void)
{
    printf(C_CYAN "TEST: determinant (dval)\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(3.0, "x");
        dval_t *y = test_dv_new_named_var_d(4.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *vals[4] = {x, one, y, two};
        matrix_t *A = mat_create_dv(2, 2, vals);
        dval_t *det = NULL;

        print_mdv("A (2x2 dval)", A);
        check_bool("mat_det(dval 2x2) rc = 0", mat_det(A, &det) == 0);
        check_bool("det(dval 2x2) non-null", det != NULL);

        if (det) {
            print_det_dv("det(A)", det);
            check_d("det [[x,1],[y,2]] at x=3,y=4 = 2", dv_eval_d(det), 2.0, 1e-12);
            check_dval_text_contains("det 2x2 contains x", det, "x");
            check_dval_text_contains("det 2x2 contains y", det, "y");

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 6.0);
            check_d("det [[x,1],[y,2]] tracks x,y", dv_eval_d(det), 4.0, 1e-12);
        }

        dv_free(det);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *z = test_dv_new_named_var_d(5.0, "z");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *zero = DV_ZERO;
        dval_t *vals[9] = {
            x,    one,  zero,
            zero, y,    one,
            one,  zero, z};
        matrix_t *A = mat_create_dv(3, 3, vals);
        dval_t *det = NULL;

        print_mdv("A (dense 3x3 dval)", A);
        check_bool("mat_det(dval dense 3x3) rc = 0", mat_det(A, &det) == 0);
        check_bool("det(dval dense 3x3) non-null", det != NULL);

        if (det) {
            print_det_dv("det(A)", det);
            check_d("det dense 3x3 at x=2,y=3,z=5 = 31", dv_eval_d(det), 31.0, 1e-12);
            check_dval_text_contains("det 3x3 contains x", det, "x");
            check_dval_text_contains("det 3x3 contains y", det, "y");
            check_dval_text_contains("det 3x3 contains z", det, "z");
        }

        dv_free(det);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(z);
        dv_free(one);
    }

    {
        dval_t *a = test_dv_new_named_var_d(2.0, "a");
        dval_t *b = test_dv_new_named_var_d(3.0, "b");
        dval_t *c = test_dv_new_named_var_d(5.0, "c");
        dval_t *d = test_dv_new_named_var_d(7.0, "d");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *zero = DV_ZERO;
        dval_t *vals[16] = {
            a,    one,  zero, zero,
            zero, b,    one,  zero,
            zero, zero, c,    one,
            zero, zero, zero, d};
        matrix_t *A = mat_create_dv(4, 4, vals);
        dval_t *det = NULL;

        print_mdv("A (upper triangular 4x4 dval)", A);
        check_bool("mat_det(dval triangular 4x4) rc = 0", mat_det(A, &det) == 0);
        check_bool("det(dval triangular 4x4) non-null", det != NULL);

        if (det) {
            print_det_dv("det(A)", det);
            check_d("det triangular 4x4 = a*b*c*d at sample point", dv_eval_d(det), 210.0, 1e-12);
            test_dv_set_val_d(a, 11.0);
            test_dv_set_val_d(b, 13.0);
            test_dv_set_val_d(c, 17.0);
            test_dv_set_val_d(d, 19.0);
            check_d("det triangular 4x4 tracks variables", dv_eval_d(det), 46189.0, 1e-9);
        }

        dv_free(det);
        mat_free(A);
        dv_free(a);
        dv_free(b);
        dv_free(c);
        dv_free(d);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(1.0, "x");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x,    one,  DV_ZERO,
            x,    one,  DV_ZERO,
            DV_ZERO, DV_ZERO, one};
        matrix_t *A = mat_create_dv(3, 3, vals);
        dval_t *det = NULL;

        print_mdv("A (singular dval)", A);
        check_bool("mat_det(dval singular) rc = 0", mat_det(A, &det) == 0);
        check_bool("det(dval singular) non-null", det != NULL);
        if (det)
            check_d("det singular dval = 0", dv_eval_d(det), 0.0, 1e-12);

        dv_free(det);
        mat_free(A);
        dv_free(x);
        dv_free(one);
    }
}

static void test_symbolic_linear_algebra_extensions(void)
{
    printf(C_CYAN "TEST: symbolic characteristic polynomial / minimal polynomial / adjugate / nullspace\n" C_RESET);

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, one, y};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *P = mat_charpoly(A);
        dval_t *c0 = NULL;
        dval_t *c1 = NULL;
        dval_t *c2 = NULL;

        print_mdv("A (charpoly dval)", A);
        check_bool("mat_charpoly(dval) not NULL", P != NULL);
        if (P) {
            check_bool("charpoly rows = n+1", mat_get_row_count(P) == 3);
            check_bool("charpoly cols = 1", mat_get_col_count(P) == 1);
            print_mdv("charpoly(A)", P);

            mat_get(P, 0, 0, &c0);
            mat_get(P, 1, 0, &c1);
            mat_get(P, 2, 0, &c2);
            check_d("charpoly coeff[0] = 1", dv_eval_d(c0), 1.0, 1e-12);
            check_d("charpoly coeff[1] = -(x+y)", dv_eval_d(c1), -5.0, 1e-12);
            check_d("charpoly coeff[2] = x*y-1", dv_eval_d(c2), 5.0, 1e-12);
            check_dval_text_contains("charpoly coeff[1] contains x", c1, "x");
            check_dval_text_contains("charpoly coeff[1] contains y", c1, "y");
            check_dval_text_contains("charpoly coeff[2] contains x", c2, "x");
            check_dval_text_contains("charpoly coeff[2] contains y", c2, "y");

            test_dv_set_val_d(x, 5.0);
            test_dv_set_val_d(y, 7.0);
            check_d("charpoly coeff[1] tracks x,y", dv_eval_d(c1), -12.0, 1e-12);
            check_d("charpoly coeff[2] tracks x,y", dv_eval_d(c2), 34.0, 1e-12);
        }

        mat_free(P);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(5.0, "y");
        dval_t *vals[9] = {
            x, DV_ZERO, DV_ZERO,
            DV_ZERO, x, DV_ZERO,
            DV_ZERO, DV_ZERO, y};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *M = mat_minpoly(A);
        matrix_t *Z = NULL;
        dval_t *c0 = NULL;
        dval_t *c1 = NULL;
        dval_t *c2 = NULL;

        print_mdv("A (minpoly repeated diagonal dval)", A);
        check_bool("mat_minpoly(repeated diagonal dval) not NULL", M != NULL);
        if (M) {
            check_bool("minpoly rows = 3", mat_get_row_count(M) == 3);
            check_bool("minpoly cols = 1", mat_get_col_count(M) == 1);
            print_mdv("minpoly(A)", M);
            mat_get(M, 0, 0, &c0);
            mat_get(M, 1, 0, &c1);
            mat_get(M, 2, 0, &c2);
            check_d("minpoly coeff[0] = 1", dv_eval_d(c0), 1.0, 1e-12);
            check_d("minpoly coeff[1] = -(x+y)", dv_eval_d(c1), -7.0, 1e-12);
            check_d("minpoly coeff[2] = x*y", dv_eval_d(c2), 10.0, 1e-12);
            check_dval_text_contains("minpoly coeff[1] contains x", c1, "x");
            check_dval_text_contains("minpoly coeff[1] contains y", c1, "y");
            test_dv_set_val_d(x, 11.0);
            test_dv_set_val_d(y, 13.0);
            check_d("minpoly coeff[1] tracks x,y", dv_eval_d(c1), -24.0, 1e-12);
            check_d("minpoly coeff[2] tracks x,y", dv_eval_d(c2), 143.0, 1e-12);

            Z = mat_apply_poly(A, M);
            check_bool("mat_apply_poly(repeated diagonal dval) not NULL", Z != NULL);
            if (Z) {
                for (size_t i = 0; i < 3; ++i) {
                    dval_t *z = NULL;
                    char label[80];
                    mat_get(Z, i, i, &z);
                    snprintf(label, sizeof(label), "minpoly(A)(repeated diag)[%zu,%zu]", i, i);
                    check_d(label, dv_eval_d(z), 0.0, 1e-12);
                }
            }
        }

        mat_free(Z);
        mat_free(M);
        mat_free(A);
        dv_free(x);
        dv_free(y);
    }

    {
        dval_t *x = test_dv_new_named_var_d(3.0, "x");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, x, one,
            DV_ZERO, DV_ZERO, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *M = mat_minpoly(A);
        matrix_t *Z = NULL;
        dval_t *c0 = NULL;
        dval_t *c1 = NULL;
        dval_t *c2 = NULL;
        dval_t *c3 = NULL;

        print_mdv("A (minpoly Jordan dval)", A);
        check_bool("mat_minpoly(Jordan 3x3 dval) not NULL", M != NULL);
        if (M) {
            check_bool("Jordan minpoly rows = 4", mat_get_row_count(M) == 4);
            check_bool("Jordan minpoly cols = 1", mat_get_col_count(M) == 1);
            print_mdv("minpoly(A)", M);
            mat_get(M, 0, 0, &c0);
            mat_get(M, 1, 0, &c1);
            mat_get(M, 2, 0, &c2);
            mat_get(M, 3, 0, &c3);
            check_d("Jordan minpoly coeff[0] = 1", dv_eval_d(c0), 1.0, 1e-12);
            check_d("Jordan minpoly coeff[1] = -3x", dv_eval_d(c1), -9.0, 1e-12);
            check_d("Jordan minpoly coeff[2] = 3x^2", dv_eval_d(c2), 27.0, 1e-12);
            check_d("Jordan minpoly coeff[3] = -x^3", dv_eval_d(c3), -27.0, 1e-12);
            check_dval_text_contains("Jordan minpoly coeff[1] contains x", c1, "x");
            test_dv_set_val_d(x, 5.0);
            check_d("Jordan minpoly coeff[1] tracks x", dv_eval_d(c1), -15.0, 1e-12);
            check_d("Jordan minpoly coeff[2] tracks x", dv_eval_d(c2), 75.0, 1e-12);
            check_d("Jordan minpoly coeff[3] tracks x", dv_eval_d(c3), -125.0, 1e-12);

            Z = mat_apply_poly(A, M);
            check_bool("mat_apply_poly(Jordan 3x3 dval) not NULL", Z != NULL);
            if (Z) {
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        dval_t *z = NULL;
                        char label[80];
                        mat_get(Z, i, j, &z);
                        snprintf(label, sizeof(label), "minpoly(A)(Jordan)[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(z), 0.0, 1e-12);
                    }
                }
            }
        }

        mat_free(Z);
        mat_free(M);
        mat_free(A);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, one, y};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *M = mat_minpoly(A);
        matrix_t *Z = NULL;
        dval_t *c0 = NULL;
        dval_t *c1 = NULL;
        dval_t *c2 = NULL;

        print_mdv("A (minpoly dense 2x2 dval)", A);
        check_bool("mat_minpoly(dense 2x2 dval) not NULL", M != NULL);
        if (M) {
            check_bool("dense 2x2 minpoly rows = 3", mat_get_row_count(M) == 3);
            check_bool("dense 2x2 minpoly cols = 1", mat_get_col_count(M) == 1);
            print_mdv("minpoly(A)", M);
            mat_get(M, 0, 0, &c0);
            mat_get(M, 1, 0, &c1);
            mat_get(M, 2, 0, &c2);
            check_d("dense 2x2 minpoly coeff[0] = 1", dv_eval_d(c0), 1.0, 1e-12);
            check_d("dense 2x2 minpoly coeff[1] = -(x+y)", dv_eval_d(c1), -5.0, 1e-12);
            check_d("dense 2x2 minpoly coeff[2] = x*y-1", dv_eval_d(c2), 5.0, 1e-12);

            Z = mat_apply_poly(A, M);
            check_bool("mat_apply_poly(dense 2x2 dval) not NULL", Z != NULL);
            if (Z) {
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        dval_t *z = NULL;
                        char label[80];
                        mat_get(Z, i, j, &z);
                        snprintf(label, sizeof(label), "minpoly(A)(dense 2x2)[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(z), 0.0, 1e-12);
                    }
                }
            }
        }

        mat_free(Z);
        mat_free(M);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, one, y};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *Adj = mat_adjugate(A);
        dval_t *e00 = NULL;
        dval_t *e01 = NULL;
        dval_t *e10 = NULL;
        dval_t *e11 = NULL;

        print_mdv("A (adjugate dval)", A);
        check_bool("mat_adjugate(dval 2x2) not NULL", Adj != NULL);
        if (Adj) {
            print_mdv("adj(A)", Adj);
            mat_get(Adj, 0, 0, &e00);
            mat_get(Adj, 0, 1, &e01);
            mat_get(Adj, 1, 0, &e10);
            mat_get(Adj, 1, 1, &e11);
            check_d("adj[0,0] = y", dv_eval_d(e00), 3.0, 1e-12);
            check_d("adj[0,1] = -1", dv_eval_d(e01), -1.0, 1e-12);
            check_d("adj[1,0] = -1", dv_eval_d(e10), -1.0, 1e-12);
            check_d("adj[1,1] = x", dv_eval_d(e11), 2.0, 1e-12);
        }

        mat_free(Adj);
        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *vals[4] = {one, two, two, four};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *Adj = mat_adjugate(A);
        matrix_t *Z = NULL;

        check_bool("mat_adjugate(singular dval 2x2) not NULL", Adj != NULL);
        if (Adj) {
            Z = mat_mul(A, Adj);
            check_bool("A*adj(A) for singular dval not NULL", Z != NULL);
            if (Z) {
                dval_t *entry = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        char label[64];
                        mat_get(Z, i, j, &entry);
                        snprintf(label, sizeof(label), "singular dval A*adj(A)[%zu,%zu]", i, j);
                        check_d(label, dv_eval_d(entry), 0.0, 1e-12);
                    }
                }
            }
        }

        mat_free(Z);
        mat_free(Adj);
        mat_free(A);
        dv_free(one);
        dv_free(two);
        dv_free(four);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *zero = DV_ZERO;
        dval_t *neg_x = dv_neg(x);
        dval_t *neg_y = dv_neg(y);
        dval_t *vals[6] = {one, zero, neg_x,
                           zero, one, neg_y};
        matrix_t *A = mat_create_dv(2, 3, vals);
        matrix_t *N = mat_nullspace(A);
        matrix_t *AN = NULL;
        dval_t *n0 = NULL;
        dval_t *n1 = NULL;
        dval_t *n2 = NULL;

        print_mdv("A (nullspace dval)", A);
        check_bool("mat_nullspace(dval) not NULL", N != NULL);
        if (N) {
            check_bool("dval nullspace rows = 3", mat_get_row_count(N) == 3);
            check_bool("dval nullspace cols = 1", mat_get_col_count(N) == 1);
            print_mdv("nullspace(A)", N);

            mat_get(N, 0, 0, &n0);
            mat_get(N, 1, 0, &n1);
            mat_get(N, 2, 0, &n2);
            check_d("nullspace basis[0] = x", dv_eval_d(n0), 2.0, 1e-12);
            check_d("nullspace basis[1] = y", dv_eval_d(n1), 3.0, 1e-12);
            check_d("nullspace basis[2] = 1", dv_eval_d(n2), 1.0, 1e-12);

            AN = mat_mul(A, N);
            check_bool("A*nullspace(dval) not NULL", AN != NULL);
            if (AN) {
                dval_t *entry = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    char label[64];
                    mat_get(AN, i, 0, &entry);
                    snprintf(label, sizeof(label), "A*nullspace(dval)[%zu,0]", i);
                    check_d(label, dv_eval_d(entry), 0.0, 1e-12);
                }
            }

            test_dv_set_val_d(x, 11.0);
            test_dv_set_val_d(y, 13.0);
            check_d("nullspace basis[0] tracks x", dv_eval_d(n0), 11.0, 1e-12);
            check_d("nullspace basis[1] tracks y", dv_eval_d(n1), 13.0, 1e-12);
            check_d("nullspace basis[2] remains 1", dv_eval_d(n2), 1.0, 1e-12);
        }

        mat_free(AN);
        mat_free(N);
        mat_free(A);
        dv_free(neg_x);
        dv_free(neg_y);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[6] = {
            one, one, x,
            one, one, y
        };
        matrix_t *A = mat_create_dv(2, 3, vals);

        print_mdv("A (rank dval)", A);
        check_bool("mat_rank(dval rectangular dependent) = 2", mat_rank(A) == 2);

        test_dv_set_val_d(x, 5.0);
        test_dv_set_val_d(y, 5.0);
        check_bool("mat_rank(dval rectangular remains exact-symbolic 2)", mat_rank(A) == 2);

        mat_free(A);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *vals[6] = {
            one, one, x,
            one, one, x
        };
        matrix_t *A = mat_create_dv(2, 3, vals);

        print_mdv("A (rank dval dependent)", A);
        check_bool("mat_rank(dval structurally dependent) = 1", mat_rank(A) == 1);

        test_dv_set_val_d(x, 5.0);
        check_bool("mat_rank(dval structurally dependent stays 1)", mat_rank(A) == 1);

        mat_free(A);
        dv_free(x);
        dv_free(one);
    }
}

/* ------------------------------------------------------------------ solve / least-squares */

static void test_solve_and_lstsq(void)
{
    printf(C_CYAN "TEST: mat_solve and mat_least_squares\n" C_RESET);

    /* Solve with pivoting and multiple RHSs. */
    {
        double A_vals[4] = {0.0, 2.0,
                            1.0, 3.0};
        double X_expected_vals[4] = {1.0, -1.0,
                                     2.0,  4.0};
        double B_vals[4] = {4.0, 8.0,
                            7.0, 11.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        matrix_t *B = test_mat_create_d(2, 2, B_vals);
        matrix_t *X_expected = test_mat_create_d(2, 2, X_expected_vals);

        print_mnum("A", A);
        print_mnum("B", B);

        matrix_t *X = mat_solve(A, B);
        check_bool("mat_solve(double) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(A,B)=X", X, X_expected, 1e-12);

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Lower-triangular direct solve. */
    {
        double L_vals[9] = {2.0, 0.0, 0.0,
                            3.0, 1.0, 0.0,
                            1.0, -2.0, 4.0};
        double X_expected_vals[3] = {1.0, 2.0, -1.0};
        double B_vals[3] = {2.0, 5.0, -7.0};
        matrix_t *L = test_mat_create_d(3, 3, L_vals);
        matrix_t *B = test_mat_create_d(3, 1, B_vals);
        matrix_t *X_expected = test_mat_create_d(3, 1, X_expected_vals);

        print_md("L (lower triangular)", L);
        print_md("B", B);

        matrix_t *X = mat_solve(L, B);
        check_bool("mat_solve(lower triangular) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(L,B)=X", X, X_expected, 1e-12);

        mat_free(L);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Upper-triangular direct solve. */
    {
        double U_vals[9] = {2.0, 1.0, -1.0,
                            0.0, 3.0, 2.0,
                            0.0, 0.0, 4.0};
        double X_expected_vals[3] = {1.0, -2.0, 0.5};
        double B_vals[3] = {-0.5, -5.0, 2.0};
        matrix_t *U = test_mat_create_d(3, 3, U_vals);
        matrix_t *B = test_mat_create_d(3, 1, B_vals);
        matrix_t *X_expected = test_mat_create_d(3, 1, X_expected_vals);

        print_md("U (upper triangular)", U);
        print_md("B", B);

        matrix_t *X = mat_solve(U, B);
        check_bool("mat_solve(upper triangular) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(U,B)=X", X, X_expected, 1e-12);

        mat_free(U);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Sparse lower-triangular solve exercises sparse-aware direct substitution. */
    {
        matrix_t *L = test_mat_sparse_d(3, 3);
        matrix_t *B = test_mat_create_d(3, 1, (double[]){4.0, 5.0, 7.0});
        matrix_t *X_expected = test_mat_create_d(3, 1, (double[]){2.0, 0.75, 1.125});
        double v;

        check_bool("sparse lower-triangular input allocated", L != NULL && B != NULL && X_expected != NULL);
        if (!L || !B || !X_expected) {
            mat_free(L);
            mat_free(B);
            mat_free(X_expected);
            return;
        }

        v = 2.0; mat_set(L, 0, 0, &v);
        v = 1.0; mat_set(L, 1, 0, &v);
        v = 4.0; mat_set(L, 1, 1, &v);
        v = -1.0; mat_set(L, 2, 0, &v);
        v = 3.0; mat_set(L, 2, 1, &v);
        v = 6.0; mat_set(L, 2, 2, &v);

        check_bool("sparse matrix recognised as lower triangular", mat_is_lower_triangular(L));
        print_md("L (sparse lower triangular)", L);
        print_md("B", B);

        matrix_t *X = mat_solve(L, B);
        check_bool("mat_solve(sparse lower triangular) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(sparse L,B)=X", X, X_expected, 1e-12);

        mat_free(L);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Diagonal solve preserves the right-hand-side layout. */
    {
        matrix_t *D = test_mat_diagonal_d(3, (double[]){2.0, 4.0, 8.0});
        matrix_t *B = test_mat_sparse_d(3, 3);
        matrix_t *X_expected = test_mat_sparse_d(3, 3);
        double v;

        check_bool("diagonal solve inputs allocated", D != NULL && B != NULL && X_expected != NULL);
        if (!D || !B || !X_expected) {
            mat_free(D);
            mat_free(B);
            mat_free(X_expected);
            return;
        }

        v = 4.0; mat_set(B, 0, 0, &v);
        v = 12.0; mat_set(B, 1, 2, &v);
        v = 16.0; mat_set(B, 2, 1, &v);

        v = 2.0; mat_set(X_expected, 0, 0, &v);
        v = 3.0; mat_set(X_expected, 1, 2, &v);
        v = 2.0; mat_set(X_expected, 2, 1, &v);

        print_md("D (diagonal)", D);
        print_md("B (sparse right-hand side)", B);

        matrix_t *X = mat_solve(D, B);
        check_bool("mat_solve(diagonal,sparse RHS) not NULL", X != NULL);
        if (X) {
            check_bool("diagonal solve preserves sparse layout of RHS", mat_is_sparse(X));
            check_mat_d("solve(D,B)=X with sparse RHS", X, X_expected, 1e-12);
        }

        mat_free(D);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* General sparse solve goes through LU plus substitution. */
    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        matrix_t *B = test_mat_create_d(3, 1, (double[]){7.0, 8.0, 3.0});
        matrix_t *X_expected = test_mat_create_d(3, 1, (double[]){7.0 / 3.0, 2.0 / 3.0, 3.0});
        double v;

        check_bool("general sparse solve inputs allocated", A != NULL && B != NULL && X_expected != NULL);
        if (!A || !B || !X_expected) {
            mat_free(A);
            mat_free(B);
            mat_free(X_expected);
            return;
        }

        v = 4.0;  mat_set(A, 0, 0, &v);
        v = 1.0;  mat_set(A, 0, 1, &v);
        v = -1.0; mat_set(A, 0, 2, &v);
        v = 2.0;  mat_set(A, 1, 0, &v);
        v = 5.0;  mat_set(A, 1, 1, &v);
        v = 1.0;  mat_set(A, 2, 2, &v);

        print_md("A (general sparse)", A);
        print_md("B", B);

        matrix_t *X = mat_solve(A, B);
        check_bool("mat_solve(general sparse) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(sparse A,B)=X", X, X_expected, 1e-12);

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* General sparse solve with pivoting exercises sparse row swaps too. */
    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        matrix_t *B = test_mat_create_d(3, 1, (double[]){4.0, 11.0, 2.0});
        matrix_t *X_expected = test_mat_create_d(3, 1, (double[]){1.0, 2.0, 2.0});
        double v;

        check_bool("general sparse pivoting solve inputs allocated", A != NULL && B != NULL && X_expected != NULL);
        if (!A || !B || !X_expected) {
            mat_free(A);
            mat_free(B);
            mat_free(X_expected);
            return;
        }

        v = 2.0; mat_set(A, 0, 1, &v);
        v = 1.0; mat_set(A, 1, 0, &v);
        v = 1.0; mat_set(A, 1, 1, &v);
        v = 1.0; mat_set(A, 2, 2, &v);
        v = 4.0; mat_set(A, 1, 2, &v);

        print_md("A (general sparse with pivoting)", A);
        print_md("B", B);

        matrix_t *X = mat_solve(A, B);
        check_bool("mat_solve(general sparse with pivoting) not NULL", X != NULL);
        if (X)
            check_mat_d("solve(sparse pivoting A,B)=X", X, X_expected, 1e-12);

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Rank-deficient overdetermined system falls back to pseudoinverse. */
    {
        number_t A_vals[6] = {
            num_create_from_long(1), num_create_from_long(0),
            num_create_from_long(2), num_create_from_long(0),
            num_create_from_long(3), num_create_from_long(0)};
        number_t B_vals[3] = {
            num_create_from_long(1),
            num_create_from_long(2),
            num_create_from_long(3)};
        number_t X_expected_vals[2] = {
            num_create_from_long(1),
            num_create_from_long(0)};
        matrix_t *A = mat_create_num(3, 2, A_vals);
        matrix_t *B = mat_create_num(3, 1, B_vals);
        matrix_t *X_expected = mat_create_num(2, 1, X_expected_vals);

        for (size_t i = 0; i < 6; ++i) num_destroy(&A_vals[i]);
        for (size_t i = 0; i < 3; ++i) num_destroy(&B_vals[i]);
        for (size_t i = 0; i < 2; ++i) num_destroy(&X_expected_vals[i]);

        print_mnum("A", A);
        print_mnum("B", B);

        matrix_t *X = mat_least_squares(A, B);
        check_bool("mat_least_squares(rank-deficient) not NULL", X != NULL);
        if (X) {
            matrix_t *Xq = test_mat_evaluate_qc(X);
            matrix_t *Xeq = test_mat_evaluate_qc(X_expected);
            check_bool("mat_least_squares(rank-deficient) -> MAT_TYPE_NUMBER",
                       mat_typeof(X) == MAT_TYPE_NUMBER);
            if (Xq && Xeq)
                check_mat_qc("rank-deficient lstsq(A,B)=X", Xq, Xeq, 1e-10);
            mat_free(Xq);
            mat_free(Xeq);
        }

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Underdetermined system returns the minimum-norm solution. */
    {
        number_t A_vals[6] = {
            num_create_from_long(1), num_create_from_long(0), num_create_from_long(0),
            num_create_from_long(0), num_create_from_long(1), num_create_from_long(0)};
        number_t B_vals[2] = {num_create_from_long(2), num_create_from_long(3)};
        number_t X_expected_vals[3] = {
            num_create_from_long(2), num_create_from_long(3), num_create_from_long(0)};
        matrix_t *A = mat_create_num(2, 3, A_vals);
        matrix_t *B = mat_create_num(2, 1, B_vals);
        matrix_t *X_expected = mat_create_num(3, 1, X_expected_vals);

        for (size_t i = 0; i < 6; ++i) num_destroy(&A_vals[i]);
        for (size_t i = 0; i < 2; ++i) num_destroy(&B_vals[i]);
        for (size_t i = 0; i < 3; ++i) num_destroy(&X_expected_vals[i]);

        print_mnum("A", A);
        print_mnum("B", B);

        matrix_t *X = mat_least_squares(A, B);
        check_bool("mat_least_squares(underdetermined) not NULL", X != NULL);
        if (X) {
            matrix_t *Xq = test_mat_evaluate_qc(X);
            matrix_t *Xeq = test_mat_evaluate_qc(X_expected);
            check_bool("mat_least_squares(underdetermined) -> MAT_TYPE_NUMBER",
                       mat_typeof(X) == MAT_TYPE_NUMBER);
            if (Xq && Xeq)
                check_mat_qc("underdetermined lstsq(A,B)=minimum-norm X", Xq, Xeq, 1e-10);
            mat_free(Xq);
            mat_free(Xeq);
        }

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Exact least-squares recovery for an overdetermined system. */
    {
        number_t A_vals[6] = {
            num_create_from_long(1), num_create_from_long(0),
            num_create_from_long(1), num_create_from_long(1),
            num_create_from_long(1), num_create_from_long(2)};
        number_t X_expected_vals[2] = {
            num_create_from_long(2), num_create_from_long(-1)};
        number_t B_vals[3] = {
            num_create_from_long(2), num_create_from_long(1), num_create_from_long(0)};
        matrix_t *A = mat_create_num(3, 2, A_vals);
        matrix_t *B = mat_create_num(3, 1, B_vals);
        matrix_t *X_expected = mat_create_num(2, 1, X_expected_vals);

        for (size_t i = 0; i < 6; ++i) num_destroy(&A_vals[i]);
        for (size_t i = 0; i < 3; ++i) num_destroy(&B_vals[i]);
        for (size_t i = 0; i < 2; ++i) num_destroy(&X_expected_vals[i]);

        print_mnum("A", A);
        print_md("B", B);

        matrix_t *X = mat_least_squares(A, B);
        check_bool("mat_least_squares(double) not NULL", X != NULL);
        if (X) {
            matrix_t *Xq = test_mat_evaluate_qc(X);
            matrix_t *Xeq = test_mat_evaluate_qc(X_expected);
            check_bool("mat_least_squares(double) -> MAT_TYPE_NUMBER",
                       mat_typeof(X) == MAT_TYPE_NUMBER);
            if (Xq && Xeq)
                check_mat_qc("lstsq(A,B)=X", Xq, Xeq, 1e-12);
            mat_free(Xq);
            mat_free(Xeq);
        }

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Exact symbolic least-squares recovery for a full-column-rank system. */
    {
        dval_t *p = test_dv_new_named_var_d(3.0, "p");
        dval_t *q = test_dv_new_named_var_d(4.0, "q");
        dval_t *u = test_dv_new_named_var_d(5.0, "u");
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *A_vals[6] = {
            p,       DV_ZERO,
            DV_ZERO, q,
            DV_ONE,  DV_ONE
        };
        dval_t *X_vals[2] = {u, two};
        matrix_t *A = mat_create_dv(3, 2, A_vals);
        matrix_t *X_expected = mat_create_dv(2, 1, X_vals);
        matrix_t *B = mat_mul(A, X_expected);
        matrix_t *X = NULL;
        matrix_t *AX = NULL;

        print_mdv("A (least-squares dval)", A);
        print_mdv("B = A*X", B);

        X = mat_least_squares(A, B);
        check_bool("mat_least_squares(dval full-column-rank) not NULL", X != NULL);
        check_bool("mat_least_squares(dval full-column-rank) -> MAT_TYPE_DVAL",
                   X != NULL && mat_typeof(X) == MAT_TYPE_DVAL);
        if (X) {
            dval_t *x00 = NULL, *x10 = NULL;

            print_mdv("X least-squares result (dval)", X);
            mat_get(X, 0, 0, &x00);
            mat_get(X, 1, 0, &x10);
            check_d("lstsq(A,B)[0,0] = u", dv_eval_d(x00), 5.0, 1e-12);
            check_d("lstsq(A,B)[1,0] = 2", dv_eval_d(x10), 2.0, 1e-12);

            test_dv_set_val_d(p, 7.0);
            test_dv_set_val_d(q, 11.0);
            test_dv_set_val_d(u, 13.0);
            check_d("lstsq(A,B)[0,0] tracks u", dv_eval_d(x00), 13.0, 1e-12);
            check_d("lstsq(A,B)[1,0] remains 2", dv_eval_d(x10), 2.0, 1e-12);

            AX = mat_mul(A, X);
            check_bool("A*lstsq(A,B) not NULL", AX != NULL);
            if (AX) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    mat_get(AX, i, 0, &got);
                    mat_get(B, i, 0, &expect);
                    check_d("dval least-squares residual entry",
                            dv_eval_d(got), dv_eval_d(expect), 1e-10);
                }
            }
        }

        mat_free(A);
        mat_free(X_expected);
        mat_free(B);
        mat_free(X);
        mat_free(AX);
        dv_free(p);
        dv_free(q);
        dv_free(u);
        dv_free(two);
    }

    /* Exact symbolic least-squares for a rank-deficient rectangular system. */
    {
        dval_t *p = test_dv_new_named_var_d(3.0, "p");
        dval_t *A_vals[6] = {
            p,      p,
            DV_ZERO, DV_ZERO,
            DV_ZERO, DV_ZERO
        };
        dval_t *X_expected_vals[2] = {DV_ONE, DV_ONE};
        matrix_t *A = mat_create_dv(3, 2, A_vals);
        matrix_t *X_expected = mat_create_dv(2, 1, X_expected_vals);
        matrix_t *B = mat_mul(A, X_expected);
        matrix_t *X = NULL;
        matrix_t *AX = NULL;

        print_mdv("A (rank-deficient least-squares dval)", A);
        print_mdv("B = A*X", B);

        X = mat_least_squares(A, B);
        check_bool("mat_least_squares(dval rank-deficient) not NULL", X != NULL);
        check_bool("mat_least_squares(dval rank-deficient) -> MAT_TYPE_DVAL",
                   X != NULL && mat_typeof(X) == MAT_TYPE_DVAL);
        if (X) {
            dval_t *x00 = NULL, *x10 = NULL;

            print_mdv("X least-squares result (rank-deficient dval)", X);
            mat_get(X, 0, 0, &x00);
            mat_get(X, 1, 0, &x10);
            check_d("rank-deficient lstsq(A,B)[0,0] = 1", dv_eval_d(x00), 1.0, 1e-12);
            check_d("rank-deficient lstsq(A,B)[1,0] = 1", dv_eval_d(x10), 1.0, 1e-12);

            test_dv_set_val_d(p, 7.0);
            check_d("rank-deficient lstsq(A,B)[0,0] stays 1", dv_eval_d(x00), 1.0, 1e-12);
            check_d("rank-deficient lstsq(A,B)[1,0] stays 1", dv_eval_d(x10), 1.0, 1e-12);

            AX = mat_mul(A, X);
            check_bool("A*lstsq(A,B) for rank-deficient dval not NULL", AX != NULL);
            if (AX) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    mat_get(AX, i, 0, &got);
                    mat_get(B, i, 0, &expect);
                    check_d("rank-deficient dval least-squares residual entry",
                            dv_eval_d(got), dv_eval_d(expect), 1e-10);
                }
            }
        }

        mat_free(A);
        mat_free(X_expected);
        mat_free(B);
        mat_free(X);
        mat_free(AX);
        dv_free(p);
    }


    /* Complex solve exercises promotion and Hermitian-free elimination. */
    {
        qcomplex_t A_vals[4] = {
            qc_make(qf_from_double(1.0), qf_from_double(1.0)),
            qc_make(qf_from_double(2.0), qf_from_double(0.0)),
            qc_make(qf_from_double(0.0), qf_from_double(1.0)),
            qc_make(qf_from_double(3.0), qf_from_double(-1.0))
        };
        qcomplex_t X_expected_vals[2] = {
            qc_make(qf_from_double(1.0), qf_from_double(-1.0)),
            qc_make(qf_from_double(2.0), qf_from_double(0.5))
        };
        matrix_t *A = test_mat_create_qc(2, 2, A_vals);
        matrix_t *X_expected = test_mat_create_qc(2, 1, X_expected_vals);
        matrix_t *B = mat_mul(A, X_expected);

        print_mqc("A", A);
        print_mqc("B", B);

        matrix_t *X = mat_solve(A, B);
        check_bool("mat_solve(qcomplex) not NULL", X != NULL);
        if (X)
            check_mat_qc("solve(A,B)=X (qcomplex)", X, X_expected, 1e-25);

        mat_free(A);
        mat_free(B);
        mat_free(X_expected);
        mat_free(X);
    }

    /* Symbolic lower-triangular solve. */
    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *z = test_dv_new_named_var_d(4.0, "z");
        dval_t *s = test_dv_new_named_var_d(5.0, "s");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *X_vals[3] = {s, two, one};
        dval_t *L_vals[9] = {
            x,       DV_ZERO, DV_ZERO,
            one,     y,       DV_ZERO,
            two,     three,   z
        };
        matrix_t *L = mat_create_dv(3, 3, L_vals);
        matrix_t *X_expected = mat_create_dv(3, 1, X_vals);
        matrix_t *B = mat_mul(L, X_expected);
        matrix_t *X = NULL;
        matrix_t *LB = NULL;

        print_mdv("L (lower triangular dval)", L);
        print_mdv("X expected", X_expected);
        print_mdv("B = L*X", B);

        X = mat_solve(L, B);
        check_bool("mat_solve(lower triangular dval) not NULL", X != NULL);
        check_bool("mat_solve(lower triangular dval) -> MAT_TYPE_DVAL",
                   X != NULL && mat_typeof(X) == MAT_TYPE_DVAL);
        if (X) {
            char *x_text = mat_to_string(X, MAT_STRING_INLINE_PRETTY);
            dval_t *x00 = NULL, *x10 = NULL, *x20 = NULL;

            print_mdv("X solve result", X);
            check_bool("solve(L,B) exact text simplified",
                       x_text && strcmp(x_text, "{ (s; 2; 1) | s = 5 }") == 0);
            mat_get(X, 0, 0, &x00);
            mat_get(X, 1, 0, &x10);
            mat_get(X, 2, 0, &x20);
            check_d("solve(L,B)[0,0] = s", dv_eval_d(x00), 5.0, 1e-12);
            check_d("solve(L,B)[1,0] = 2", dv_eval_d(x10), 2.0, 1e-12);
            check_d("solve(L,B)[2,0] = 1", dv_eval_d(x20), 1.0, 1e-12);

            test_dv_set_val_d(x, 7.0);
            test_dv_set_val_d(y, 11.0);
            test_dv_set_val_d(z, 13.0);
            test_dv_set_val_d(s, 17.0);
            check_d("solve(L,B)[0,0] tracks s only", dv_eval_d(x00), 17.0, 1e-12);
            check_d("solve(L,B)[1,0] remains 2", dv_eval_d(x10), 2.0, 1e-12);
            check_d("solve(L,B)[2,0] remains 1", dv_eval_d(x20), 1.0, 1e-12);

            LB = mat_mul(L, X);
            check_bool("L*solve(L,B) not NULL", LB != NULL);
            if (LB) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    mat_get(LB, i, 0, &got);
                    mat_get(B, i, 0, &expect);
                    check_d("lower triangular dval residual row", dv_eval_d(got), dv_eval_d(expect), 1e-10);
                }
            }

            free(x_text);
        }

        mat_free(L);
        mat_free(X_expected);
        mat_free(B);
        mat_free(X);
        mat_free(LB);
        dv_free(x);
        dv_free(y);
        dv_free(z);
        dv_free(s);
        dv_free(one);
        dv_free(two);
        dv_free(three);
    }

    /* General dense symbolic solve with multiple right-hand sides. */
    {
        dval_t *a = test_dv_new_named_var_d(4.0, "a");
        dval_t *b = test_dv_new_named_var_d(5.0, "b");
        dval_t *c = test_dv_new_named_var_d(6.0, "c");
        dval_t *u = test_dv_new_named_var_d(2.0, "u");
        dval_t *v = test_dv_new_named_var_d(3.0, "v");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *A_vals[9] = {
            a,    one,  DV_ZERO,
            one,  b,    one,
            DV_ZERO, one, c
        };
        dval_t *X_vals[6] = {
            u,    one,
            two,  v,
            three, four
        };
        matrix_t *A = mat_create_dv(3, 3, A_vals);
        matrix_t *X_expected = mat_create_dv(3, 2, X_vals);
        matrix_t *B = mat_mul(A, X_expected);
        matrix_t *X = NULL;
        matrix_t *AX = NULL;

        print_mdv("A (dense dval)", A);
        print_mdv("X expected", X_expected);
        print_mdv("B = A*X", B);

        X = mat_solve(A, B);
        check_bool("mat_solve(dense dval) not NULL", X != NULL);
        check_bool("mat_solve(dense dval) -> MAT_TYPE_DVAL",
                   X != NULL && mat_typeof(X) == MAT_TYPE_DVAL);
        if (X) {
            dval_t *x00 = NULL, *x01 = NULL, *x11 = NULL, *x20 = NULL;

            print_mdv("X solve result (dense dval)", X);
            mat_get(X, 0, 0, &x00);
            mat_get(X, 0, 1, &x01);
            mat_get(X, 1, 1, &x11);
            mat_get(X, 2, 0, &x20);
            check_d("solve(A,B)[0,0] = u", dv_eval_d(x00), 2.0, 1e-12);
            check_d("solve(A,B)[0,1] = 1", dv_eval_d(x01), 1.0, 1e-12);
            check_d("solve(A,B)[1,1] = v", dv_eval_d(x11), 3.0, 1e-12);
            check_d("solve(A,B)[2,0] = 3", dv_eval_d(x20), 3.0, 1e-12);

            test_dv_set_val_d(a, 7.0);
            test_dv_set_val_d(b, 8.0);
            test_dv_set_val_d(c, 9.0);
            test_dv_set_val_d(u, 11.0);
            test_dv_set_val_d(v, 13.0);
            check_d("solve(A,B)[0,0] tracks u", dv_eval_d(x00), 11.0, 1e-12);
            check_d("solve(A,B)[0,1] remains 1", dv_eval_d(x01), 1.0, 1e-12);
            check_d("solve(A,B)[1,1] tracks v", dv_eval_d(x11), 13.0, 1e-12);
            check_d("solve(A,B)[2,0] remains 3", dv_eval_d(x20), 3.0, 1e-12);

            AX = mat_mul(A, X);
            check_bool("A*solve(A,B) not NULL", AX != NULL);
            if (AX) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        mat_get(AX, i, j, &got);
                        mat_get(B, i, j, &expect);
                        check_d("dense dval residual entry", dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }
        }

        mat_free(A);
        mat_free(X_expected);
        mat_free(B);
        mat_free(X);
        mat_free(AX);
        dv_free(a);
        dv_free(b);
        dv_free(c);
        dv_free(u);
        dv_free(v);
        dv_free(one);
        dv_free(two);
        dv_free(three);
        dv_free(four);
    }

    /* Larger dense symbolic solve with multiple right-hand sides. */
    {
        dval_t *a = test_dv_new_named_var_d(5.0, "a");
        dval_t *b = test_dv_new_named_var_d(6.0, "b");
        dval_t *c = test_dv_new_named_var_d(7.0, "c");
        dval_t *d = test_dv_new_named_var_d(8.0, "d");
        dval_t *e = test_dv_new_named_var_d(9.0, "e");
        dval_t *f = test_dv_new_named_var_d(10.0, "f");
        dval_t *u = test_dv_new_named_var_d(11.0, "u");
        dval_t *v = test_dv_new_named_var_d(13.0, "v");
        dval_t *one = test_dv_new_const_d(1.0);
        dval_t *two = test_dv_new_const_d(2.0);
        dval_t *three = test_dv_new_const_d(3.0);
        dval_t *four = test_dv_new_const_d(4.0);
        dval_t *five = test_dv_new_const_d(5.0);
        dval_t *six = test_dv_new_const_d(6.0);
        dval_t *seven = test_dv_new_const_d(7.0);
        dval_t *A_vals[36] = {
            a,    one,  two,  DV_ZERO, DV_ZERO, DV_ZERO,
            one,  b,    one,  DV_ZERO, DV_ZERO, DV_ZERO,
            two,  one,  c,    one,     DV_ZERO, DV_ZERO,
            DV_ZERO, DV_ZERO, one,  d,    one,  two,
            DV_ZERO, DV_ZERO, DV_ZERO, one,  e,    one,
            DV_ZERO, DV_ZERO, DV_ZERO, two,  one,  f
        };
        dval_t *X_vals[12] = {
            u,     one,
            two,   v,
            three, four,
            four,  five,
            five,  six,
            six,   seven
        };
        matrix_t *A = mat_create_dv(6, 6, A_vals);
        matrix_t *X_expected = mat_create_dv(6, 2, X_vals);
        matrix_t *B = mat_mul(A, X_expected);
        matrix_t *X = NULL;
        matrix_t *AX = NULL;

        print_mdv("A (dense 6x6 dval)", A);
        print_mdv("B = A*X", B);

        X = mat_solve(A, B);
        check_bool("mat_solve(dense 6x6 dval) not NULL", X != NULL);
        check_bool("mat_solve(dense 6x6 dval) -> MAT_TYPE_DVAL",
                   X != NULL && mat_typeof(X) == MAT_TYPE_DVAL);
        if (X) {
            dval_t *x00 = NULL, *x11 = NULL, *x32 = NULL;

            mat_get(X, 0, 0, &x00);
            mat_get(X, 1, 1, &x11);
            mat_get(X, 5, 1, &x32);
            check_d("solve(A,B)[0,0] = u", dv_eval_d(x00), 11.0, 1e-12);
            check_d("solve(A,B)[1,1] = v", dv_eval_d(x11), 13.0, 1e-12);
            check_d("solve(A,B)[5,1] = 7", dv_eval_d(x32), 7.0, 1e-12);

            test_dv_set_val_d(a, 15.0);
            test_dv_set_val_d(b, 16.0);
            test_dv_set_val_d(c, 17.0);
            test_dv_set_val_d(d, 18.0);
            test_dv_set_val_d(e, 19.0);
            test_dv_set_val_d(f, 20.0);
            test_dv_set_val_d(u, 23.0);
            test_dv_set_val_d(v, 29.0);
            check_d("solve(A,B)[0,0] tracks u on 6x6", dv_eval_d(x00), 23.0, 1e-12);
            check_d("solve(A,B)[1,1] tracks v on 6x6", dv_eval_d(x11), 29.0, 1e-12);
            check_d("solve(A,B)[5,1] remains 7 on 6x6", dv_eval_d(x32), 7.0, 1e-12);

            AX = mat_mul(A, X);
            check_bool("A*solve(A,B) 6x6 not NULL", AX != NULL);
            if (AX) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 6; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        mat_get(AX, i, j, &got);
                        mat_get(B, i, j, &expect);
                        check_d("dense 6x6 dval solve residual entry",
                                dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }
        }

        mat_free(A);
        mat_free(X_expected);
        mat_free(B);
        mat_free(X);
        mat_free(AX);
        dv_free(a);
        dv_free(b);
        dv_free(c);
        dv_free(d);
        dv_free(e);
        dv_free(f);
        dv_free(u);
        dv_free(v);
        dv_free(one);
        dv_free(two);
        dv_free(three);
        dv_free(four);
        dv_free(five);
        dv_free(six);
        dv_free(seven);
    }
}

static void test_factorisations(void)
{
    printf(C_CYAN "TEST: LU / QR / Cholesky / SVD / Schur\n" C_RESET);

    {
        double A_vals[4] = {0.0, 2.0,
                            1.0, 3.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        mat_lu_factor_t lu = {0};
        matrix_t *PA = NULL, *LU = NULL;

        print_md("A", A);
        check_bool("mat_lu_factor(double) rc=0", mat_lu_factor(A, &lu) == 0);
        check_bool("LU factors non-null", lu.P && lu.L && lu.U);
        check_bool("L is lower triangular", lu.L && mat_is_lower_triangular(lu.L));
        check_bool("U is upper triangular", lu.U && mat_is_upper_triangular(lu.U));
        if (lu.P && lu.L && lu.U) {
            PA = mat_mul(lu.P, A);
            LU = mat_mul(lu.L, lu.U);
            check_bool("P*A not NULL", PA != NULL);
            check_bool("L*U not NULL", LU != NULL);
            if (PA && LU)
                check_mat_d("P*A = L*U", PA, LU, 1e-12);
        }

        mat_free(PA);
        mat_free(LU);
        mat_lu_factor_free(&lu);
        mat_free(A);
    }

    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        mat_lu_factor_t lu = {0};
        double v;

        check_bool("sparse LU input allocated", A != NULL);
        if (!A) {
            return;
        }

        v = 4.0;  mat_set(A, 0, 0, &v);
        v = 1.0;  mat_set(A, 0, 1, &v);
        v = -1.0; mat_set(A, 0, 2, &v);
        v = 2.0;  mat_set(A, 1, 0, &v);
        v = 5.0;  mat_set(A, 1, 1, &v);
        v = 1.0;  mat_set(A, 2, 2, &v);

        print_md("A (sparse)", A);
        check_bool("mat_lu_factor(sparse) rc=0", mat_lu_factor(A, &lu) == 0);
        check_bool("sparse LU factors non-null", lu.P && lu.L && lu.U);
        check_bool("sparse LU P uses sparse storage", lu.P && mat_is_sparse(lu.P));
        check_bool("sparse LU permutation nonzero count = n",
                   lu.P && mat_nonzero_count(lu.P) == 3);
        check_bool("sparse LU L uses sparse storage", lu.L && mat_is_sparse(lu.L));
        check_bool("sparse LU U uses sparse storage", lu.U && mat_is_sparse(lu.U));
        check_bool("sparse LU L is lower triangular", lu.L && mat_is_lower_triangular(lu.L));
        check_bool("sparse LU U is upper triangular", lu.U && mat_is_upper_triangular(lu.U));
        if (lu.P && lu.L && lu.U) {
            matrix_t *PA = mat_mul(lu.P, A);
            matrix_t *LU = mat_mul(lu.L, lu.U);
            check_bool("sparse P*A not NULL", PA != NULL);
            check_bool("sparse L*U not NULL", LU != NULL);
            if (PA && LU)
                check_mat_d("sparse P*A = L*U", PA, LU, 1e-12);
            mat_free(PA);
            mat_free(LU);
        }

        mat_lu_factor_free(&lu);
        mat_free(A);
    }

    {
        double A_vals[6] = {1.0, 1.0,
                            1.0, 0.0,
                            0.0, 1.0};
        matrix_t *A = test_mat_create_d(3, 2, A_vals);
        mat_qr_factor_t qr = {0};
        matrix_t *QR = NULL, *QH = NULL, *QtQ = NULL;

        print_md("A", A);
        check_bool("mat_qr_factor(double) rc=0", mat_qr_factor(A, &qr) == 0);
        check_bool("QR factors non-null", qr.Q && qr.R);
        check_bool("QR Q uses number_t", qr.Q && mat_typeof(qr.Q) == MAT_TYPE_NUMBER);
        check_bool("QR R uses number_t", qr.R && mat_typeof(qr.R) == MAT_TYPE_NUMBER);
        check_bool("R is upper triangular", qr.R && mat_is_upper_triangular(qr.R));
        if (qr.Q && qr.R) {
            matrix_t *QRq = NULL, *Aqc = NULL, *QtQq = NULL, *Iq = NULL, *Iqq = NULL;
            QR = mat_mul(qr.Q, qr.R);
            QH = mat_hermitian(qr.Q);
            QtQ = QH ? mat_mul(QH, qr.Q) : NULL;
            check_bool("Q*R not NULL", QR != NULL);
            check_bool("Q*Q not NULL", QtQ != NULL);
            QRq = QR ? test_mat_evaluate_qc(QR) : NULL;
            Aqc = test_mat_evaluate_qc(A);
            Iq = mat_create_identity_num(2);
            QtQq = QtQ ? test_mat_evaluate_qc(QtQ) : NULL;
            Iqq = Iq ? test_mat_evaluate_qc(Iq) : NULL;
            if (QRq && Aqc)
                check_mat_qc("Q*R = A", QRq, Aqc, 1e-12);
            if (QtQq && Iqq)
                check_mat_qc("Q^T Q = I", QtQq, Iqq, 1e-12);
            mat_free(QRq);
            mat_free(Aqc);
            mat_free(QtQq);
            mat_free(Iq);
            mat_free(Iqq);
        }

        mat_free(QR);
        mat_free(QH);
        mat_free(QtQ);
        mat_qr_factor_free(&qr);
        mat_free(A);
    }

    {
        double A_vals[9] = {4.0, 1.0, 1.0,
                            1.0, 3.0, 0.5,
                            1.0, 0.5, 2.0};
        matrix_t *A = test_mat_create_d(3, 3, A_vals);
        mat_cholesky_t chol = {0};
        matrix_t *LH = NULL, *LLH = NULL;

        print_md("A", A);
        check_bool("mat_cholesky(double) rc=0", mat_cholesky(A, &chol) == 0);
        check_bool("Cholesky factor non-null", chol.L != NULL);
        check_bool("Cholesky factor uses number_t", chol.L && mat_typeof(chol.L) == MAT_TYPE_NUMBER);
        check_bool("Cholesky factor is lower triangular", chol.L && mat_is_lower_triangular(chol.L));
        if (chol.L) {
            matrix_t *LLHq = NULL, *Aqc = NULL;
            LH = mat_hermitian(chol.L);
            LLH = LH ? mat_mul(chol.L, LH) : NULL;
            check_bool("L*L^T not NULL", LLH != NULL);
            LLHq = LLH ? test_mat_evaluate_qc(LLH) : NULL;
            Aqc = test_mat_evaluate_qc(A);
            if (LLHq && Aqc)
                check_mat_qc("L*L^T = A", LLHq, Aqc, 1e-12);
            mat_free(LLHq);
            mat_free(Aqc);
        }

        mat_free(LH);
        mat_free(LLH);
        mat_cholesky_free(&chol);
        mat_free(A);
    }

    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        mat_cholesky_t chol = {0};
        matrix_t *LH = NULL, *LLH = NULL;
        double v;

        check_bool("sparse Cholesky input allocated", A != NULL);
        if (!A) {
            return;
        }

        v = 4.0; mat_set(A, 0, 0, &v);
        v = 1.0; mat_set(A, 0, 1, &v);
        v = 1.0; mat_set(A, 1, 0, &v);
        v = 3.0; mat_set(A, 1, 1, &v);
        v = 0.5; mat_set(A, 1, 2, &v);
        v = 0.5; mat_set(A, 2, 1, &v);
        v = 2.0; mat_set(A, 2, 2, &v);

        print_md("A (sparse SPD)", A);
        check_bool("mat_cholesky(sparse) rc=0", mat_cholesky(A, &chol) == 0);
        check_bool("sparse Cholesky factor non-null", chol.L != NULL);
        check_bool("sparse Cholesky factor uses number_t", chol.L && mat_typeof(chol.L) == MAT_TYPE_NUMBER);
        check_bool("sparse Cholesky factor uses sparse storage", chol.L && mat_is_sparse(chol.L));
        check_bool("sparse Cholesky factor is lower triangular", chol.L && mat_is_lower_triangular(chol.L));
        if (chol.L) {
            matrix_t *LLHq = NULL, *Aqc = NULL;
            LH = mat_hermitian(chol.L);
            LLH = LH ? mat_mul(chol.L, LH) : NULL;
            check_bool("sparse L*L^T not NULL", LLH != NULL);
            LLHq = LLH ? test_mat_evaluate_qc(LLH) : NULL;
            Aqc = test_mat_evaluate_qc(A);
            if (LLHq && Aqc)
                check_mat_qc("sparse L*L^T = A", LLHq, Aqc, 1e-12);
            mat_free(LLHq);
            mat_free(Aqc);
        }

        mat_free(LH);
        mat_free(LLH);
        mat_cholesky_free(&chol);
        mat_free(A);
    }

    {
        matrix_t *A = test_mat_sparse_d(3, 3);
        mat_lu_factor_t lu = {0};
        double v;

        check_bool("sparse pivoting LU input allocated", A != NULL);
        if (!A) {
            return;
        }

        v = 2.0; mat_set(A, 0, 1, &v);
        v = 1.0; mat_set(A, 1, 0, &v);
        v = 1.0; mat_set(A, 1, 1, &v);
        v = 4.0; mat_set(A, 1, 2, &v);
        v = 1.0; mat_set(A, 2, 2, &v);

        print_md("A (sparse pivoting LU)", A);
        check_bool("mat_lu_factor(sparse pivoting) rc=0", mat_lu_factor(A, &lu) == 0);
        check_bool("sparse pivoting LU factors non-null", lu.P && lu.L && lu.U);
        check_bool("sparse pivoting LU P uses sparse storage", lu.P && mat_is_sparse(lu.P));
        check_bool("sparse pivoting permutation nonzero count = n",
                   lu.P && mat_nonzero_count(lu.P) == 3);
        if (lu.P && lu.L && lu.U) {
            matrix_t *PA = mat_mul(lu.P, A);
            matrix_t *LU = mat_mul(lu.L, lu.U);
            check_bool("sparse pivoting P*A not NULL", PA != NULL);
            check_bool("sparse pivoting L*U not NULL", LU != NULL);
            if (PA && LU)
                check_mat_d("sparse pivoting P*A = L*U", PA, LU, 1e-12);
            mat_free(PA);
            mat_free(LU);
        }

        mat_lu_factor_free(&lu);
        mat_free(A);
    }

    {
        qcomplex_t A_vals[4] = {
            qc_make(qf_from_double(3.0), QF_ZERO),
            qc_make(qf_from_double(1.0), qf_from_double(1.0)),
            qc_make(qf_from_double(1.0), qf_from_double(-1.0)),
            qc_make(qf_from_double(2.0), QF_ZERO)
        };
        matrix_t *A = test_mat_create_qc(2, 2, A_vals);
        mat_cholesky_t chol = {0};
        matrix_t *LH = NULL, *LLH = NULL;

        print_mqc("A", A);
        check_bool("mat_cholesky(qcomplex) rc=0", mat_cholesky(A, &chol) == 0);
        check_bool("qcomplex Cholesky factor non-null", chol.L != NULL);
        check_bool("qcomplex Cholesky factor uses number_t", chol.L && mat_typeof(chol.L) == MAT_TYPE_NUMBER);
        check_bool("qcomplex Cholesky factor is lower triangular", chol.L && mat_is_lower_triangular(chol.L));
        if (chol.L) {
            matrix_t *LLHq = NULL, *Aqc = NULL;
            LH = mat_hermitian(chol.L);
            LLH = LH ? mat_mul(chol.L, LH) : NULL;
            check_bool("qcomplex L*L* not NULL", LLH != NULL);
            LLHq = LLH ? test_mat_evaluate_qc(LLH) : NULL;
            Aqc = test_mat_evaluate_qc(A);
            if (LLHq && Aqc)
                check_mat_qc("L*L* = A (qcomplex)", LLHq, Aqc, 1e-25);
            mat_free(LLHq);
            mat_free(Aqc);
        }

        mat_free(LH);
        mat_free(LLH);
        mat_cholesky_free(&chol);
        mat_free(A);
    }

    {
        double A_vals[6] = {3.0, 0.0,
                            0.0, 2.0,
                            0.0, 0.0};
        matrix_t *A = test_mat_create_d(3, 2, A_vals);
        mat_svd_factor_t svd = {0};
        matrix_t *US = NULL, *VH = NULL, *USVH = NULL, *UH = NULL, *UHU = NULL, *VHV = NULL;

        print_md("A", A);
        check_bool("mat_svd_factor(double) rc=0", mat_svd_factor(A, &svd) == 0);
        check_bool("SVD factors non-null", svd.U && svd.S && svd.V);
        check_bool("SVD U uses number_t", svd.U && mat_typeof(svd.U) == MAT_TYPE_NUMBER);
        check_bool("SVD S uses number_t", svd.S && mat_typeof(svd.S) == MAT_TYPE_NUMBER);
        check_bool("SVD V uses number_t", svd.V && mat_typeof(svd.V) == MAT_TYPE_NUMBER);
        check_bool("S is diagonal", svd.S && mat_is_diagonal(svd.S));
        if (svd.U && svd.S && svd.V) {
            matrix_t *USVHq = NULL, *Aqc = NULL, *UHUq = NULL, *VHVq = NULL;
            matrix_t *Iq = NULL, *Iqq = NULL;
            US = mat_mul(svd.U, svd.S);
            VH = mat_hermitian(svd.V);
            USVH = (US && VH) ? mat_mul(US, VH) : NULL;
            UH = mat_hermitian(svd.U);
            UHU = UH ? mat_mul(UH, svd.U) : NULL;
            VHV = VH ? mat_mul(VH, svd.V) : NULL;
            check_bool("U*S*V^T not NULL", USVH != NULL);
            check_bool("U^T U not NULL", UHU != NULL);
            check_bool("V^T V not NULL", VHV != NULL);
            USVHq = USVH ? test_mat_evaluate_qc(USVH) : NULL;
            Aqc = test_mat_evaluate_qc(A);
            UHUq = UHU ? test_mat_evaluate_qc(UHU) : NULL;
            VHVq = VHV ? test_mat_evaluate_qc(VHV) : NULL;
            Iq = mat_create_identity_num(2);
            Iqq = Iq ? test_mat_evaluate_qc(Iq) : NULL;
            if (USVHq && Aqc)
                check_mat_qc("U*S*V^T = A", USVHq, Aqc, 1e-10);
            if (UHUq && Iqq)
                check_mat_qc("U^T U = I", UHUq, Iqq, 1e-10);
            if (VHVq && Iqq)
                check_mat_qc("V^T V = I", VHVq, Iqq, 1e-10);
            mat_free(USVHq);
            mat_free(Aqc);
            mat_free(UHUq);
            mat_free(VHVq);
            mat_free(Iq);
            mat_free(Iqq);
        }

        mat_free(US);
        mat_free(VH);
        mat_free(USVH);
        mat_free(UH);
        mat_free(UHU);
        mat_free(VHV);
        mat_svd_factor_free(&svd);
        mat_free(A);
    }

    {
        double A_vals[4] = {4.0, -1.0,
                            2.0,  1.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        mat_schur_factor_t schur = {0};
        matrix_t *QT = NULL, *QH = NULL, *QTQH = NULL, *QHQ = NULL;

        print_md("A", A);
        check_bool("mat_schur_factor(double) rc=0", mat_schur_factor(A, &schur) == 0);
        check_bool("Schur factors non-null", schur.Q && schur.T);
        check_bool("Schur T is upper triangular", schur.T && mat_is_upper_triangular(schur.T));
        check_bool("Schur Q uses number_t", schur.Q && mat_typeof(schur.Q) == MAT_TYPE_NUMBER);
        check_bool("Schur T uses number_t", schur.T && mat_typeof(schur.T) == MAT_TYPE_NUMBER);
        if (schur.Q && schur.T) {
            QT = mat_mul(schur.Q, schur.T);
            QH = mat_hermitian(schur.Q);
            QTQH = (QT && QH) ? mat_mul(QT, QH) : NULL;
            QHQ = QH ? mat_mul(QH, schur.Q) : NULL;

            check_bool("Q*T*Q^H not NULL", QTQH != NULL);
            check_bool("Q^H*Q not NULL", QHQ != NULL);
            if (QTQH) {
                number_t aq_vals[4] = {
                    num_create_from_double(4.0),
                    num_create_from_double(-1.0),
                    num_create_from_double(2.0),
                    num_create_from_double(1.0)
                };
                matrix_t *Aq = mat_create_num(2, 2, aq_vals);
                check_mat_num_local("Q*T*Q^H = A", QTQH, Aq, 1e-24);
                for (size_t i = 0; i < 4; ++i)
                    num_destroy(&aq_vals[i]);
                mat_free(Aq);
            }
            if (QHQ) {
                number_t iq_vals[4] = {
                    num_create_from_long(1), num_create_from_long(0),
                    num_create_from_long(0), num_create_from_long(1)
                };
                matrix_t *Iq = mat_create_num(2, 2, iq_vals);
                matrix_t *QHQq = QHQ ? test_mat_evaluate_qc(QHQ) : NULL;
                matrix_t *Iqq = Iq ? test_mat_evaluate_qc(Iq) : NULL;
                check_bool("Q^H*Q qc view not NULL", QHQq != NULL && Iqq != NULL);
                if (QHQq && Iqq)
                    check_mat_qc("Q^H*Q = I", QHQq, Iqq, 1e-24);
                for (size_t i = 0; i < 4; ++i)
                    num_destroy(&iq_vals[i]);
                mat_free(QHQq);
                mat_free(Iqq);
                mat_free(Iq);
            }

            for (size_t i = 1; i < 2; i++) {
                for (size_t j = 0; j < i; j++) {
                    number_t tij = mat_get_num(schur.T, i, j);
                    number_t abs_tij = num_abs(tij);
                    check_bool("Schur T entry below diagonal is zero",
                               num_to_double(abs_tij) < 1e-24);
                    num_destroy(&abs_tij);
                    num_destroy(&tij);
                }
            }
        }

        mat_free(QT);
        mat_free(QH);
        mat_free(QTQH);
        mat_free(QHQ);
        mat_schur_factor_free(&schur);
        mat_free(A);
    }
}

static void test_rank_pinv_nullspace(void)
{
    printf(C_CYAN "TEST: rank / pseudoinverse / nullspace\n" C_RESET);

    {
        number_t A_vals[9] = {
            num_create_from_long(1), num_create_from_long(2), num_create_from_long(3),
            num_create_from_long(2), num_create_from_long(4), num_create_from_long(6),
            num_create_from_long(1), num_create_from_long(1), num_create_from_long(1)
        };
        matrix_t *A = mat_create_num(3, 3, A_vals);
        matrix_t *N = NULL;
        matrix_t *AN = NULL;

        for (size_t i = 0; i < 9; ++i) num_destroy(&A_vals[i]);

        print_md("A", A);
        check_bool("mat_rank(A)=2", mat_rank(A) == 2);

        N = mat_nullspace(A);
        check_bool("mat_nullspace(A) not NULL", N != NULL);
        if (N) {
            check_bool("mat_nullspace(A) -> MAT_TYPE_NUMBER",
                       mat_typeof(N) == MAT_TYPE_NUMBER);
            check_bool("nullspace rows = 3", mat_get_row_count(N) == 3);
            check_bool("nullspace cols = 1", mat_get_col_count(N) == 1);
            print_mnum("nullspace(A)", N);
            AN = mat_mul(A, N);
            check_bool("A*nullspace(A) not NULL", AN != NULL);
            if (AN) {
                number_t zero_data[3] = { NUM_ZERO, NUM_ZERO, NUM_ZERO };
                matrix_t *Z = mat_create_num(3, 1, zero_data);
                check_mat_num_local("A*nullspace(A)=0", AN, Z, 1e-10);
                mat_free(Z);
            }
        }

        mat_free(AN);
        mat_free(N);
        mat_free(A);
    }

    {
        number_t A_vals[8] = {
            num_create_from_long(1), num_create_from_long(0), num_create_from_long(1), num_create_from_long(0),
            num_create_from_long(0), num_create_from_long(1), num_create_from_long(0), num_create_from_long(1)
        };
        number_t pinv_vals[8] = {
            num_create_from_string("1/2"), num_create_from_long(0),
            num_create_from_long(0),      num_create_from_string("1/2"),
            num_create_from_string("1/2"), num_create_from_long(0),
            num_create_from_long(0),      num_create_from_string("1/2")
        };
        matrix_t *A = mat_create_num(2, 4, A_vals);
        matrix_t *A_pinv_expected = mat_create_num(4, 2, pinv_vals);
        matrix_t *A_pinv = NULL;
        matrix_t *N = NULL;
        matrix_t *AN = NULL;
        matrix_t *AAp = NULL, *AApA = NULL;
        matrix_t *ApA = NULL, *ApAAp = NULL;

        for (size_t i = 0; i < 8; ++i) num_destroy(&A_vals[i]);
        for (size_t i = 0; i < 8; ++i) num_destroy(&pinv_vals[i]);

        print_mnum("A", A);
        check_bool("mat_rank(wide A)=2", mat_rank(A) == 2);

        A_pinv = mat_pseudoinverse(A);
        check_bool("mat_pseudoinverse(A) not NULL", A_pinv != NULL);
        if (A_pinv) {
            matrix_t *Apq = test_mat_evaluate_qc(A_pinv);
            matrix_t *Apeq = test_mat_evaluate_qc(A_pinv_expected);
            matrix_t *AApAq = NULL, *Aq = test_mat_evaluate_qc(A);
            matrix_t *ApAApq = NULL;
            print_mnum("pinv(A)", A_pinv);
            check_bool("mat_pseudoinverse(A) -> MAT_TYPE_NUMBER",
                       mat_typeof(A_pinv) == MAT_TYPE_NUMBER);
            if (Apq && Apeq)
                check_mat_qc("pinv(A)=expected", Apq, Apeq, 1e-10);

            AAp = mat_mul(A, A_pinv);
            AApA = AAp ? mat_mul(AAp, A) : NULL;
            ApA = mat_mul(A_pinv, A);
            ApAAp = ApA ? mat_mul(ApA, A_pinv) : NULL;
            check_bool("A*A+*A not NULL", AApA != NULL);
            check_bool("A+*A*A+ not NULL", ApAAp != NULL);
            AApAq = AApA ? test_mat_evaluate_qc(AApA) : NULL;
            ApAApq = ApAAp ? test_mat_evaluate_qc(ApAAp) : NULL;
            if (AApAq && Aq)
                check_mat_qc("A*A+*A=A", AApAq, Aq, 1e-10);
            if (ApAApq && Apq)
                check_mat_qc("A+*A*A+=A+", ApAApq, Apq, 1e-10);
            mat_free(Apq);
            mat_free(Apeq);
            mat_free(AApAq);
            mat_free(Aq);
            mat_free(ApAApq);
        }

        N = mat_nullspace(A);
        check_bool("mat_nullspace(wide A) not NULL", N != NULL);
        if (N) {
            matrix_t *ANq = NULL, *Zq = NULL;
            check_bool("mat_nullspace(wide A) -> MAT_TYPE_NUMBER",
                       mat_typeof(N) == MAT_TYPE_NUMBER);
            check_bool("wide nullspace rows = 4", mat_get_row_count(N) == 4);
            check_bool("wide nullspace cols = 2", mat_get_col_count(N) == 2);
            print_mnum("nullspace(A)", N);
            AN = mat_mul(A, N);
            check_bool("A*nullspace(wide A) not NULL", AN != NULL);
            if (AN) {
                number_t zero_data[4] = { NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO };
                matrix_t *Z = mat_create_num(2, 2, zero_data);
                ANq = test_mat_evaluate_qc(AN);
                Zq = Z ? test_mat_evaluate_qc(Z) : NULL;
                if (ANq && Zq)
                    check_mat_qc("A*nullspace(A)=0 (wide)", ANq, Zq, 1e-10);
                mat_free(ANq);
                mat_free(Zq);
                mat_free(Z);
            }
        }

        mat_free(AN);
        mat_free(N);
        mat_free(AAp);
        mat_free(AApA);
        mat_free(ApA);
        mat_free(ApAAp);
        mat_free(A_pinv);
        mat_free(A_pinv_expected);
        mat_free(A);
    }

    {
        dval_t *p = test_dv_new_named_var_d(2.0, "p");
        dval_t *q = test_dv_new_named_var_d(3.0, "q");
        dval_t *A_vals[6] = {
            p,       DV_ZERO, DV_ONE,
            DV_ZERO, q,       DV_ONE
        };
        matrix_t *A = mat_create_dv(2, 3, A_vals);
        matrix_t *A_pinv = NULL;
        matrix_t *AAp = NULL, *AApA = NULL;
        matrix_t *ApA = NULL, *ApAAp = NULL;

        print_mdv("A (wide dval)", A);
        check_bool("mat_rank(wide dval)=2", mat_rank(A) == 2);

        A_pinv = mat_pseudoinverse(A);
        check_bool("mat_pseudoinverse(wide dval) not NULL", A_pinv != NULL);
        check_bool("mat_pseudoinverse(wide dval) -> MAT_TYPE_DVAL",
                   A_pinv != NULL && mat_typeof(A_pinv) == MAT_TYPE_DVAL);
        if (A_pinv) {
            dval_t *entry = NULL;

            print_mdv("pinv(A dval)", A_pinv);
            mat_get(A_pinv, 0, 0, &entry);
            check_d("pinv(dval)[0,0] initial", dv_eval_d(entry), 20.0 / 49.0, 1e-12);

            AAp = mat_mul(A, A_pinv);
            AApA = AAp ? mat_mul(AAp, A) : NULL;
            ApA = mat_mul(A_pinv, A);
            ApAAp = ApA ? mat_mul(ApA, A_pinv) : NULL;
            check_bool("A*A+ for wide dval not NULL", AAp != NULL);
            check_bool("A*A+*A for wide dval not NULL", AApA != NULL);
            check_bool("A+*A*A+ for wide dval not NULL", ApAAp != NULL);
            if (AAp) {
                dval_t *got = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        double expect = (i == j) ? 1.0 : 0.0;
                        mat_get(AAp, i, j, &got);
                        check_d("wide dval A*A+ identity entry",
                                dv_eval_d(got), expect, 1e-10);
                    }
                }
            }
            if (AApA) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        mat_get(AApA, i, j, &got);
                        mat_get(A, i, j, &expect);
                        check_d("wide dval A*A+*A = A entry",
                                dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }
            if (ApAAp) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        mat_get(ApAAp, i, j, &got);
                        mat_get(A_pinv, i, j, &expect);
                        check_d("wide dval A+*A*A+ = A+ entry",
                                dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }

            test_dv_set_val_d(p, 5.0);
            test_dv_set_val_d(q, 7.0);
            check_d("pinv(dval)[0,0] tracks p,q", dv_eval_d(entry), 250.0 / 1299.0, 1e-12);
            if (AAp) {
                dval_t *got = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        double expect = (i == j) ? 1.0 : 0.0;
                        mat_get(AAp, i, j, &got);
                        check_d("wide dval A*A+ identity entry tracks updates",
                                dv_eval_d(got), expect, 1e-10);
                    }
                }
            }
        }

        mat_free(AAp);
        mat_free(AApA);
        mat_free(ApA);
        mat_free(ApAAp);
        mat_free(A_pinv);
        mat_free(A);
        dv_free(p);
        dv_free(q);
    }

    {
        dval_t *p = test_dv_new_named_var_d(2.0, "p");
        dval_t *A_vals[6] = {
            p,       DV_ZERO, p,
            DV_ZERO, DV_ZERO, DV_ZERO
        };
        matrix_t *A = mat_create_dv(2, 3, A_vals);
        matrix_t *A_pinv = NULL;
        matrix_t *AAp = NULL;
        matrix_t *AApA = NULL;
        matrix_t *ApA = NULL;
        matrix_t *ApAAp = NULL;

        print_mdv("A (rank-deficient wide dval)", A);
        check_bool("mat_rank(rank-deficient wide dval)=1", mat_rank(A) == 1);

        A_pinv = mat_pseudoinverse(A);
        check_bool("mat_pseudoinverse(rank-deficient wide dval) not NULL", A_pinv != NULL);
        check_bool("mat_pseudoinverse(rank-deficient wide dval) -> MAT_TYPE_DVAL",
                   A_pinv != NULL && mat_typeof(A_pinv) == MAT_TYPE_DVAL);
        if (A_pinv) {
            dval_t *entry = NULL;
            dval_t *zero_entry = NULL;

            print_mdv("pinv(rank-deficient wide dval)", A_pinv);
            mat_get(A_pinv, 0, 0, &entry);
            mat_get(A_pinv, 1, 0, &zero_entry);
            check_d("rank-deficient pinv(dval)[0,0] initial", dv_eval_d(entry), 0.25, 1e-12);
            check_d("rank-deficient pinv(dval)[1,0] initial", dv_eval_d(zero_entry), 0.0, 1e-12);

            AAp = mat_mul(A, A_pinv);
            AApA = AAp ? mat_mul(AAp, A) : NULL;
            ApA = mat_mul(A_pinv, A);
            ApAAp = ApA ? mat_mul(ApA, A_pinv) : NULL;
            check_bool("rank-deficient dval A*A+*A not NULL", AApA != NULL);
            check_bool("rank-deficient dval A+*A*A+ not NULL", ApAAp != NULL);
            if (AApA) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        mat_get(AApA, i, j, &got);
                        mat_get(A, i, j, &expect);
                        check_d("rank-deficient wide dval A*A+*A = A entry",
                                dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }
            if (ApAAp) {
                dval_t *got = NULL, *expect = NULL;
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 2; ++j) {
                        mat_get(ApAAp, i, j, &got);
                        mat_get(A_pinv, i, j, &expect);
                        check_d("rank-deficient wide dval A+*A*A+ = A+ entry",
                                dv_eval_d(got), dv_eval_d(expect), 1e-10);
                    }
                }
            }

            test_dv_set_val_d(p, 5.0);
            check_d("rank-deficient pinv(dval)[0,0] tracks p", dv_eval_d(entry), 0.1, 1e-12);
        }

        mat_free(AAp);
        mat_free(AApA);
        mat_free(ApA);
        mat_free(ApAAp);
        mat_free(A_pinv);
        mat_free(A);
        dv_free(p);
    }

}

static void test_norms_and_condition(void)
{
    printf(C_CYAN "TEST: matrix norms and condition number\n" C_RESET);

    {
        double A_vals[4] = {
            3.0, 0.0,
            0.0, 4.0
        };
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        qfloat_t out = QF_ZERO;

        print_md("A", A);

        check_bool("mat_norm(A,1)=0", mat_norm(A, MAT_NORM_1, &out) == 0);
        check_qf_val("||A||_1 = 4", out, qf_from_double(4.0), 1e-30);

        check_bool("mat_norm(A,inf)=0", mat_norm(A, MAT_NORM_INF, &out) == 0);
        check_qf_val("||A||_inf = 4", out, qf_from_double(4.0), 1e-30);

        check_bool("mat_norm(A,F)=0", mat_norm(A, MAT_NORM_FRO, &out) == 0);
        check_qf_val("||A||_F = 5", out, qf_from_double(5.0), 1e-30);

        check_bool("mat_norm(A,2)=0", mat_norm(A, MAT_NORM_2, &out) == 0);
        check_qf_val("||A||_2 = 4", out, qf_from_double(4.0), 1e-30);

        check_bool("mat_condition_number(A,1)=0", mat_condition_number(A, MAT_NORM_1, &out) == 0);
        check_qf_val("cond_1(A) = 4/3", out, qf_div(qf_from_double(4.0), qf_from_double(3.0)), 1e-28);

        check_bool("mat_condition_number(A,inf)=0", mat_condition_number(A, MAT_NORM_INF, &out) == 0);
        check_qf_val("cond_inf(A) = 4/3", out, qf_div(qf_from_double(4.0), qf_from_double(3.0)), 1e-28);

        check_bool("mat_condition_number(A,2)=0", mat_condition_number(A, MAT_NORM_2, &out) == 0);
        check_qf_val("cond_2(A) = 4/3", out, qf_div(qf_from_double(4.0), qf_from_double(3.0)), 1e-28);

        check_bool("mat_condition_number(A,F)=0", mat_condition_number(A, MAT_NORM_FRO, &out) == 0);
        check_qf_val("cond_F(A) = 25/12", out,
                     qf_div(qf_from_double(25.0), qf_from_double(12.0)), 1e-28);

        mat_free(A);
    }

    {
        qcomplex_t A_vals[4] = {
            qc_make(qf_from_double(3.0), qf_from_double(4.0)),
            QC_ZERO,
            QC_ZERO,
            QC_ZERO
        };
        matrix_t *A = test_mat_create_qc(2, 2, A_vals);
        qfloat_t out = QF_ZERO;

        print_mqc("A", A);

        check_bool("mat_norm(qc A,1)=0", mat_norm(A, MAT_NORM_1, &out) == 0);
        check_qf_val("||A||_1 (qc) = 5", out, qf_from_double(5.0), 1e-28);

        check_bool("mat_norm(qc A,inf)=0", mat_norm(A, MAT_NORM_INF, &out) == 0);
        check_qf_val("||A||_inf (qc) = 5", out, qf_from_double(5.0), 1e-28);

        check_bool("mat_norm(qc A,F)=0", mat_norm(A, MAT_NORM_FRO, &out) == 0);
        check_qf_val("||A||_F (qc) = 5", out, qf_from_double(5.0), 1e-28);

        check_bool("mat_norm(qc A,2)=0", mat_norm(A, MAT_NORM_2, &out) == 0);
        check_qf_val("||A||_2 (qc) = 5", out, qf_from_double(5.0), 1e-28);

        mat_free(A);
    }

    {
        double A_vals[4] = {
            1.0, 2.0,
            2.0, 4.0
        };
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        qfloat_t out = QF_ZERO;

        print_md("A", A);
        check_bool("mat_condition_number(singular A,2)=0", mat_condition_number(A, MAT_NORM_2, &out) == 0);
        check_bool("cond_2(singular A) = inf", qf_isinf(out));

        mat_free(A);
    }
}

/* ------------------------------------------------------------------ Hermitian (conjugate transpose) */

static void test_hermitian_op(void)
{
    printf(C_CYAN "TEST: Hermitian operator (A -> A^†)\n" C_RESET);

    /* double: Hermitian = transpose */
    {
        matrix_t *A = test_mat_dense_d(2, 3);
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
        matrix_t *A = test_mat_create_qc(2, 2, A_vals);

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

void run_matrix_core_tests(void)
{
    RUN_TEST_CASE(test_creation);
    RUN_TEST_CASE(test_reading);
    RUN_TEST_CASE(test_writing);
    RUN_TEST_CASE(test_number_creation_and_readback);
    RUN_TEST_CASE(test_number_special_constructors);
    RUN_TEST_CASE(test_number_matrix_arithmetic);
    RUN_TEST_CASE(test_number_det_and_inverse);
    RUN_TEST_CASE(test_mixed_number_backend_matrices);
    RUN_TEST_CASE(test_dval_multiply);
    RUN_TEST_CASE(test_dval_symbolic_printing);
    RUN_TEST_CASE(test_add_sub);
    RUN_TEST_CASE(test_multiply);
    RUN_TEST_CASE(test_transpose_conjugate);
    RUN_TEST_CASE(test_identity_get);
    RUN_TEST_CASE(test_identity_set);
    RUN_TEST_CASE(test_dval_storage_lifecycle_regressions);
    RUN_TEST_CASE(test_owned_element_reads_and_transforms);
    RUN_TEST_CASE(test_sparse_support);
    RUN_TEST_CASE(test_structural_queries_and_diagonal_construction);
    RUN_TEST_CASE(test_layout_policy_regressions);
    RUN_TEST_CASE(test_add_sub_qf);
    RUN_TEST_CASE(test_add_sub_qc);
    RUN_TEST_CASE(test_multiply_qf);
    RUN_TEST_CASE(test_multiply_qc);
    RUN_TEST_CASE(test_add_mixed_d_qf);
    RUN_TEST_CASE(test_add_mixed_d_qc);
    RUN_TEST_CASE(test_add_mixed_qf_qc);
    RUN_TEST_CASE(test_sub_mixed_d_qf);
    RUN_TEST_CASE(test_sub_mixed_d_qc);
    RUN_TEST_CASE(test_sub_mixed_qf_qc);
    RUN_TEST_CASE(test_multiply_mixed_d_qf);
    RUN_TEST_CASE(test_multiply_mixed_d_qc);
    RUN_TEST_CASE(test_multiply_mixed_qf_qc);
    RUN_TEST_CASE(test_scalar_mul_d_d);
    RUN_TEST_CASE(test_scalar_mul_d_qf);
    RUN_TEST_CASE(test_scalar_mul_d_qc);
    RUN_TEST_CASE(test_scalar_mul_qf_d);
    RUN_TEST_CASE(test_scalar_mul_qc_qc);
    RUN_TEST_CASE(test_identity_arith_d);
    RUN_TEST_CASE(test_identity_arith_qf);
    RUN_TEST_CASE(test_identity_arith_qc);
    RUN_TEST_CASE(test_scalar_div_d_d);
    RUN_TEST_CASE(test_scalar_div_qf_qf);
    RUN_TEST_CASE(test_scalar_div_qc_qc);
    RUN_TEST_CASE(test_scalar_div_d_qf);
    RUN_TEST_CASE(test_scalar_div_qf_d);
    RUN_TEST_CASE(test_det_double);
    RUN_TEST_CASE(test_det_qfloat);
    RUN_TEST_CASE(test_det_qcomplex);
    RUN_TEST_CASE(test_det_dval);
    RUN_TEST_CASE(test_symbolic_linear_algebra_extensions);
    RUN_TEST_CASE(test_trace);
    RUN_TEST_CASE(test_deriv);
    RUN_TEST_CASE(test_matrix_calculus);
    RUN_TEST_CASE(test_deriv_solve);
    RUN_TEST_CASE(test_deriv_block_solve);
    RUN_TEST_CASE(test_jacobian);
    RUN_TEST_CASE(test_schur_complement);
    RUN_TEST_CASE(test_block_linear_algebra);
    RUN_TEST_CASE(test_evaluate_bridge);
    RUN_TEST_CASE(test_inverse_double);
    RUN_TEST_CASE(test_inverse_qfloat);
    RUN_TEST_CASE(test_inverse_qcomplex);
    RUN_TEST_CASE(test_inverse_dval);
    RUN_TEST_CASE(test_solve_and_lstsq);
    RUN_TEST_CASE(test_factorisations);
    RUN_TEST_CASE(test_rank_pinv_nullspace);
    RUN_TEST_CASE(test_norms_and_condition);
    RUN_TEST_CASE(test_hermitian_op);
}
