#include "test_matrix.h"

#define GAMMAINV_HP_INPUT_TEXT                                                                                         \
    "1."                                                                                                               \
    "3293403881791370204736256125058588870981620920917903461603558423896834634432741360312129925539084990621701177182" \
    "11927999677114649293316951893820282202090301346528273989828842137443879771713119671699071534450972100130979"

/*
 * EXPR_ZERO / EXPR_ONE are read-only immortal nodes in the public API now.
 * These matrix test fixtures still pass them through mutable expr_t * arrays,
 * so cast locally here instead of weakening library constness.
 */
#define EXPR_ZERO ((expr_t *)EXPR_ZERO)
#define EXPR_ONE ((expr_t *)EXPR_ONE)

static void check_num_close_local(const char *label, number_t got, number_t expected, double tol)
{
    number_t diff = num_sub(got, expected);
    number_t mag = num_abs(diff);
    check_bool(label, fabs(num_to_double(mag)) <= tol);
    num_destroy(&mag);
    num_destroy(&diff);
}

typedef matrix_t *(*matrix_unary_function_t)(const matrix_t *);
typedef number_t (*number_unary_function_t)(const number_t);

typedef struct {
    const char *name;
    matrix_unary_function_t matrix_function;
    number_unary_function_t number_function;
    double input;
} matrix_number_function_case_t;

static void test_number_function_matrix_parity(void)
{
    static const matrix_number_function_case_t cases[] = {
        {"sec", mat_sec, num_sec, 0.7},
        {"cosec", mat_cosec, num_cosec, 0.7},
        {"cot", mat_cot, num_cot, 0.7},
        {"versin", mat_versin, num_versin, 0.7},
        {"vercos", mat_vercos, num_vercos, 0.7},
        {"coversin", mat_coversin, num_coversin, 0.7},
        {"covercos", mat_covercos, num_covercos, 0.7},
        {"haversin", mat_haversin, num_haversin, 0.7},
        {"havercos", mat_havercos, num_havercos, 0.7},
        {"hacoversin", mat_hacoversin, num_hacoversin, 0.7},
        {"hacovercos", mat_hacovercos, num_hacovercos, 0.7},
        {"sech", mat_sech, num_sech, 0.7},
        {"cosech", mat_cosech, num_cosech, 0.7},
        {"coth", mat_coth, num_coth, 0.7},
        {"cubrt", mat_cubrt, num_cubrt, 8.0},
        {"asec", mat_asec, num_asec, 2.0},
        {"acosec", mat_acosec, num_acosec, 2.0},
        {"acot", mat_acot, num_acot, 2.0},
        {"arcversin", mat_arcversin, num_arcversin, 0.25},
        {"arcvercos", mat_arcvercos, num_arcvercos, 0.25},
        {"arccoversin", mat_arccoversin, num_arccoversin, 0.25},
        {"arccovercos", mat_arccovercos, num_arccovercos, 0.25},
        {"archaversin", mat_archaversin, num_archaversin, 0.25},
        {"archavercos", mat_archavercos, num_archavercos, 0.25},
        {"archacoversin", mat_archacoversin, num_archacoversin, 0.25},
        {"archacovercos", mat_archacovercos, num_archacovercos, 0.25},
        {"asech", mat_asech, num_asech, 0.5},
        {"acosech", mat_acosech, num_acosech, 2.0},
        {"acoth", mat_acoth, num_acoth, 2.0},
        {"zeta", mat_zeta, num_zeta, 2.5},
        {"zetap", mat_zetap, num_zetap, 2.5},
        {"dilog", mat_dilog, num_dilog, 0.25},
        {"polylog1", mat_polylog1, num_polylog1, 0.25},
    };

    printf(C_CYAN "TEST: Number analytic functions are available as matrix functions\n" C_RESET);
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        number_t input = num_create_from_double(cases[i].input);
        number_t expected = cases[i].number_function(input);
        matrix_t *A = mat_create(1u, 1u, &input);
        matrix_t *result = cases[i].matrix_function(A);
        char label[128];

        snprintf(label, sizeof(label), "mat_%s(1x1) is available", cases[i].name);
        check_bool(label, result != NULL);
        if (result) {
            number_t got = mat_get_num(result, 0u, 0u);

            snprintf(label, sizeof(label), "mat_%s(1x1) matches num_%s", cases[i].name, cases[i].name);
            check_num_close_local(label, got, expected, 1e-12);
            num_destroy(&got);
        }

        mat_free(result);
        mat_free(A);
        num_destroy(&expected);
        num_destroy(&input);
    }
}

static void test_mat_harmonic_poly(void)
{
    number_t values[] = {num_create_from_long(1L), num_create_from_long(0L), num_create_from_long(0L),
                         num_create_from_long(2L)};
    matrix_t *A = mat_create(2u, 2u, values);
    matrix_t *result = mat_harmonic_poly(A, 2u);
    number_t value;

    printf(C_CYAN "TEST: harmonic matrix polynomial\n" C_RESET);
    ASSERT_NOT_NULL(result);
    if (result) {
        number_t expected = num_create_from_string("3/2");

        value = mat_get_num(result, 0u, 0u);
        check_bool("H_2(A)[0,0] = 3/2", num_eq(value, expected));
        num_destroy(&value);
        num_destroy(&expected);
        expected = num_create_from_long(4L);
        value = mat_get_num(result, 1u, 1u);
        check_bool("H_2(A)[1,1] = 4", num_eq(value, expected));
        num_destroy(&value);
        num_destroy(&expected);
    }

    mat_free(result);
    mat_free(A);
    for (size_t i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
        num_destroy(&values[i]);

    {
        number_t half = num_create_from_string("1/2");
        number_t two = num_create_from_long(2L);
        number_t one = num_create_from_long(1L);
        matrix_t *Z = mat_create(1u, 1u, &half);
        matrix_t *phi = mat_lerch_phi(Z, &two, &one);
        number_t got = phi ? mat_get_num(phi, 0u, 0u) : num_clone(NUM_NAN);
        number_t expected = number_lerch_phi(half, two, one);
        number_t delta = num_sub(got, expected);
        number_t error = num_abs(delta);
        number_t tolerance = num_create_from_string("1e-28");

        ASSERT_NOT_NULL(phi);
        check_bool("matrix Lerch phi agrees with its scalar value", num_lt(error, tolerance));
        num_destroy(&error);
        num_destroy(&delta);
        num_destroy(&expected);
        num_destroy(&got);
        mat_free(phi);
        mat_free(Z);

        Z = mat_create(1u, 1u, &one);
        if (Z) {
            matrix_t *psi_q = mat_qdigamma(Z, &half);
            number_t matrix_value = psi_q ? mat_get_num(psi_q, 0u, 0u) : num_clone(NUM_NAN);
            number_t scalar_value = num_qdigamma(half, one);
            number_t delta = num_sub(matrix_value, scalar_value);
            number_t difference = num_abs(delta);

            ASSERT_NOT_NULL(psi_q);
            check_bool("matrix q-digamma agrees with its scalar value", num_lt(difference, tolerance));
            num_destroy(&difference);
            num_destroy(&delta);
            num_destroy(&scalar_value);
            num_destroy(&matrix_value);
            mat_free(psi_q);
            mat_free(Z);
        }
        num_destroy(&tolerance);
        num_destroy(&one);
        num_destroy(&two);
        num_destroy(&half);
    }
}

static void test_eigen_d(void)
{
    printf(C_CYAN "TEST: eigendecomposition (double)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    double A_vals[4] = {5, 2, 2, 8};
    matrix_t *A = test_mat_create_d(2, 2, A_vals);

    print_md("A", A);

    /* eigenvalues only */
    number_t ev[2] = {NUM_ZERO, NUM_ZERO};
    mat_eigenvalues(A, ev);
    num_printf("    eigenvalue[0] (mat_eigenvalues): %N\n", ev[0]);
    num_printf("    eigenvalue[1] (mat_eigenvalues): %N\n", ev[1]);

    double lmin = fmin(num_to_double(ev[0]), num_to_double(ev[1]));
    double lmax = fmax(num_to_double(ev[0]), num_to_double(ev[1]));
    check_d("eigenvalue min = 4", lmin, 4.0, 1e-10);
    check_d("eigenvalue max = 9", lmax, 9.0, 1e-10);

    /* full decomposition */
    number_t ev2[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_mnum("eigenvectors V (columns)", V);
    num_printf("    eigenvalue2[0] (mat_eigendecompose): %N\n", ev2[0]);
    num_printf("    eigenvalue2[1] (mat_eigendecompose): %N\n", ev2[1]);
    check_bool("eigenvectors V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    /* verify A*v[k] = lambda[k]*v[k] for each column k */
    if (V) {
        for (size_t k = 0; k < 2; k++) {
            for (size_t i = 0; i < 2; i++) {
                number_t Av_ik = num_create_from_long(0);
                number_t vik = mat_get_num(V, i, k);
                for (size_t j = 0; j < 2; j++) {
                    number_t aij = mat_get_num(A, i, j);
                    number_t vjk = mat_get_num(V, j, k);
                    number_t term = num_mul(aij, vjk);
                    number_t next = num_add(Av_ik, term);
                    num_destroy(&aij);
                    num_destroy(&vjk);
                    num_destroy(&term);
                    num_destroy(&Av_ik);
                    Av_ik = next;
                }

                number_t expected = num_mul(ev2[k], vik);
                char label[64];
                snprintf(label, sizeof(label), "d: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
                check_num_close_local(label, Av_ik, expected, 1e-10);
                num_destroy(&expected);
                num_destroy(&vik);
                num_destroy(&Av_ik);
            }
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mnum("eigenvectors (mat_eigenvectors)", V2);
    check_bool("eigenvectors V2 type is number", V2 && mat_typeof(V2) == MAT_TYPE_NUMBER);

    num_destroy(&ev[0]);
    num_destroy(&ev[1]);
    num_destroy(&ev2[0]);
    num_destroy(&ev2[1]);
    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qfloat */

static void test_eigen_mp_real(void)
{
    printf(C_CYAN "TEST: eigendecomposition (mp-real)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    number_t A_vals[4] = {num_create_from_long(5), num_create_from_long(2), num_create_from_long(2),
                          num_create_from_long(8)};
    matrix_t *A = mat_create_num(2, 2, A_vals);

    print_mnum("A", A);

    /* eigenvalues only */
    number_t ev[2] = {NUM_ZERO, NUM_ZERO};
    mat_eigenvalues(A, ev);
    num_printf("    eigenvalue[0]: %N\n", ev[0]);
    num_printf("    eigenvalue[1]: %N\n", ev[1]);

    int e0_smaller = num_lt(ev[0], ev[1]);
    number_t ev_min = e0_smaller ? num_clone(ev[0]) : num_clone(ev[1]);
    number_t ev_max = e0_smaller ? num_clone(ev[1]) : num_clone(ev[0]);
    {
        number_t four = num_create_from_long(4);
        number_t nine = num_create_from_long(9);
        check_num_close_local("eigenvalue min = 4", ev_min, four, 1e-25);
        check_num_close_local("eigenvalue max = 9", ev_max, nine, 1e-25);
        num_destroy(&four);
        num_destroy(&nine);
    }

    /* full decomposition */
    number_t ev2[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_mnum("eigenvectors V (columns)", V);
    num_printf("    eigenvalue2[0]: %N\n", ev2[0]);
    num_printf("    eigenvalue2[1]: %N\n", ev2[1]);
    check_bool("mp-real eigenvectors V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (V) {
        for (size_t k = 0; k < 2; k++) {
            for (size_t i = 0; i < 2; i++) {
                number_t Av_ik = num_create_from_long(0);
                number_t vik = mat_get_num(V, i, k);
                for (size_t j = 0; j < 2; j++) {
                    number_t aij = mat_get_num(A, i, j);
                    number_t vjk = mat_get_num(V, j, k);
                    number_t term = num_mul(aij, vjk);
                    number_t next = num_add(Av_ik, term);
                    num_destroy(&aij);
                    num_destroy(&vjk);
                    num_destroy(&term);
                    num_destroy(&Av_ik);
                    Av_ik = next;
                }
                number_t expected = num_mul(ev2[k], vik);
                char label[64];
                snprintf(label, sizeof(label), "mp-real: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
                check_num_close_local(label, Av_ik, expected, 1e-25);
                num_destroy(&expected);
                num_destroy(&vik);
                num_destroy(&Av_ik);
            }
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mnum("eigenvectors (mat_eigenvectors)", V2);
    check_bool("mp-real eigenvectors V2 type is number", V2 && mat_typeof(V2) == MAT_TYPE_NUMBER);

    num_destroy(&ev_min);
    num_destroy(&ev_max);
    num_destroy(&ev[0]);
    num_destroy(&ev[1]);
    num_destroy(&ev2[0]);
    num_destroy(&ev2[1]);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);
    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qcomplex */

static void test_eigen_complex(void)
{
    printf(C_CYAN "TEST: eigendecomposition (complex Hermitian)\n" C_RESET);

    /* A = [[2, 1+i], [1-i, 3]] — eigenvalues 1 and 4 */
    number_t A_vals[4] = {num_create_from_long(2), num_create_from_string("1 + i"), num_create_from_string("1 - i"),
                          num_create_from_long(3)};
    matrix_t *A = mat_create_num(2, 2, A_vals);
    print_mnum("A", A);

    /* eigenvalues only */
    number_t ev[2] = {NUM_ZERO, NUM_ZERO};
    mat_eigenvalues(A, ev);
    num_printf("    eigenvalue[0]: %N\n", ev[0]);
    num_printf("    eigenvalue[1]: %N\n", ev[1]);

    int e0_smaller = num_lt(ev[0], ev[1]);
    number_t ev_min = e0_smaller ? num_clone(ev[0]) : num_clone(ev[1]);
    number_t ev_max = e0_smaller ? num_clone(ev[1]) : num_clone(ev[0]);
    {
        number_t one = num_create_from_long(1);
        number_t four = num_create_from_long(4);
        check_num_close_local("eigenvalue min = 1", ev_min, one, 1e-25);
        check_num_close_local("eigenvalue max = 4", ev_max, four, 1e-25);
        num_destroy(&one);
        num_destroy(&four);
    }

    /* full decomposition */
    number_t ev2[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);
    print_mnum("eigenvectors V (columns)", V);
    num_printf("    eigenvalue2[0]: %N\n", ev2[0]);
    num_printf("    eigenvalue2[1]: %N\n", ev2[1]);
    check_bool("complex eigenvectors V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (V) {
        for (size_t k = 0; k < 2; k++) {
            for (size_t i = 0; i < 2; i++) {
                number_t Av_ik = num_create_from_long(0);
                number_t vik = mat_get_num(V, i, k);
                for (size_t j = 0; j < 2; j++) {
                    number_t aij = mat_get_num(A, i, j);
                    number_t vjk = mat_get_num(V, j, k);
                    number_t term = num_mul(aij, vjk);
                    number_t next = num_add(Av_ik, term);
                    num_destroy(&aij);
                    num_destroy(&vjk);
                    num_destroy(&term);
                    num_destroy(&Av_ik);
                    Av_ik = next;
                }
                number_t expected = num_mul(ev2[k], vik);
                char label[64];
                snprintf(label, sizeof(label), "complex: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
                check_num_close_local(label, Av_ik, expected, 1e-25);
                num_destroy(&expected);
                num_destroy(&vik);
                num_destroy(&Av_ik);
            }
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mnum("eigenvectors (mat_eigenvectors)", V2);
    check_bool("complex eigenvectors V2 type is number", V2 && mat_typeof(V2) == MAT_TYPE_NUMBER);

    num_destroy(&ev_min);
    num_destroy(&ev_max);
    num_destroy(&ev[0]);
    num_destroy(&ev[1]);
    num_destroy(&ev2[0]);
    num_destroy(&ev2[1]);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);
    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

static void test_eigen_num_hermitian(void)
{
    printf(C_CYAN "TEST: eigendecomposition (number Hermitian)\n" C_RESET);

    number_t A_vals[4] = {num_create_from_long(2), num_create_from_string("1 + i"), num_create_from_string("1 - i"),
                          num_create_from_long(3)};
    matrix_t *A = mat_create_num(2, 2, A_vals);
    number_t ev[2] = {NUM_ZERO, NUM_ZERO};
    number_t ev2[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    matrix_t *V2 = NULL;

    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);

    print_mnum("A", A);

    mat_eigenvalues(A, ev);
    num_printf("    eigenvalue[0]: %N\n", ev[0]);
    num_printf("    eigenvalue[1]: %N\n", ev[1]);

    {
        int e0_smaller = num_lt(ev[0], ev[1]);
        number_t ev_min = e0_smaller ? num_clone(ev[0]) : num_clone(ev[1]);
        number_t ev_max = e0_smaller ? num_clone(ev[1]) : num_clone(ev[0]);
        number_t one = num_create_from_long(1);
        number_t four = num_create_from_long(4);
        check_num_close_local("number Hermitian eigenvalue min = 1", ev_min, one, 1e-25);
        check_num_close_local("number Hermitian eigenvalue max = 4", ev_max, four, 1e-25);
        num_destroy(&one);
        num_destroy(&four);
        num_destroy(&ev_min);
        num_destroy(&ev_max);
    }

    check_bool("number Hermitian eigendecompose rc = 0", mat_eigendecompose(A, ev2, &V) == 0);
    check_bool("number Hermitian eigenvectors V not NULL", V != NULL);
    check_bool("number Hermitian eigenvectors V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (V) {
        print_mnum("V", V);
        for (size_t k = 0; k < 2; k++) {
            for (size_t i = 0; i < 2; i++) {
                number_t Av_ik = num_create_from_long(0);
                number_t vik = mat_get_num(V, i, k);
                for (size_t j = 0; j < 2; j++) {
                    number_t aij = mat_get_num(A, i, j);
                    number_t vjk = mat_get_num(V, j, k);
                    number_t term = num_mul(aij, vjk);
                    number_t next = num_add(Av_ik, term);
                    num_destroy(&aij);
                    num_destroy(&vjk);
                    num_destroy(&term);
                    num_destroy(&Av_ik);
                    Av_ik = next;
                }

                number_t expected = num_mul(ev2[k], vik);
                char label[80];
                snprintf(label, sizeof(label), "number Hermitian: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
                check_num_close_local(label, Av_ik, expected, 1e-25);
                num_destroy(&expected);
                num_destroy(&vik);
                num_destroy(&Av_ik);
            }
        }
    }

    V2 = mat_eigenvectors(A);
    print_mnum("eigenvectors (mat_eigenvectors)", V2);
    check_bool("number Hermitian eigenvectors V2 type is number", V2 && mat_typeof(V2) == MAT_TYPE_NUMBER);

    num_destroy(&ev[0]);
    num_destroy(&ev[1]);
    num_destroy(&ev2[0]);
    num_destroy(&ev2[1]);
    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

static void test_eigen_num_hermitian_high_precision(void)
{
    printf(C_CYAN "TEST: eigendecomposition (number Hermitian, high precision)\n" C_RESET);

    number_t A_vals[4] = {num_create_from_string("2.0"), num_create_from_string("1.0 + 1.0i"),
                          num_create_from_string("1.0 - 1.0i"), num_create_from_string("3.0")};
    matrix_t *A = NULL;
    number_t ev[2] = {NUM_ZERO, NUM_ZERO};
    number_t ev2[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;

    for (size_t i = 0; i < 4; ++i)
        num_set_prec_bits(&A_vals[i], 512u);

    A = mat_create_num(2, 2, A_vals);
    check_bool("high-precision Hermitian A not NULL", A != NULL);
    check_bool("high-precision Hermitian A type is number", A && mat_typeof(A) == MAT_TYPE_NUMBER);
    if (A) {
        number_t a01 = mat_get_num(A, 0, 1);
        check_bool("high-precision Hermitian fixture preserves precision bits", num_get_prec_bits(a01) >= 512u);
        num_destroy(&a01);
    }

    print_mnum("A (high-precision Hermitian)", A);

    if (A)
        mat_eigenvalues(A, ev);
    {
        int e0_smaller = num_lt(ev[0], ev[1]);
        number_t ev_min = e0_smaller ? num_clone(ev[0]) : num_clone(ev[1]);
        number_t ev_max = e0_smaller ? num_clone(ev[1]) : num_clone(ev[0]);
        number_t one = num_create_from_long(1);
        number_t four = num_create_from_long(4);
        check_num_close_local("high-precision Hermitian eigenvalue min = 1", ev_min, one, 1e-27);
        check_num_close_local("high-precision Hermitian eigenvalue max = 4", ev_max, four, 1e-27);
        num_destroy(&one);
        num_destroy(&four);
        num_destroy(&ev_min);
        num_destroy(&ev_max);
    }

    check_bool("high-precision Hermitian eigendecompose rc = 0", A && mat_eigendecompose(A, ev2, &V) == 0);
    check_bool("high-precision Hermitian eigenvectors not NULL", V != NULL);
    check_bool("high-precision Hermitian eigenvectors type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (A && V) {
        print_mnum("V (high-precision Hermitian)", V);
        for (size_t k = 0; k < 2; ++k) {
            for (size_t i = 0; i < 2; ++i) {
                number_t Av_ik = num_create_from_long(0);
                number_t vik = mat_get_num(V, i, k);
                for (size_t j = 0; j < 2; ++j) {
                    number_t aij = mat_get_num(A, i, j);
                    number_t vjk = mat_get_num(V, j, k);
                    number_t term = num_mul(aij, vjk);
                    number_t next = num_add(Av_ik, term);
                    num_destroy(&aij);
                    num_destroy(&vjk);
                    num_destroy(&term);
                    num_destroy(&Av_ik);
                    Av_ik = next;
                }

                number_t expected = num_mul(ev2[k], vik);
                char label[96];
                snprintf(label, sizeof(label), "high-precision Hermitian: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
                check_num_close_local(label, Av_ik, expected, 1e-27);
                num_destroy(&expected);
                num_destroy(&vik);
                num_destroy(&Av_ik);
            }
        }
    }

    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);
    num_destroy(&ev[0]);
    num_destroy(&ev[1]);
    num_destroy(&ev2[0]);
    num_destroy(&ev2[1]);
    mat_free(A);
    mat_free(V);
}

/* ------------------------------------------------------------------ eigenvalues: expr */

static void check_expr_eigen_relation(const char *label_prefix, const matrix_t *A, expr_t **ev, const matrix_t *V,
                                      double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    matrix_t *D = mat_create_diagonal_expr(rows, ev);
    matrix_t *AV = mat_mul(A, V);
    matrix_t *VD = mat_mul(V, D);
    matrix_t *AVq = test_mat_evaluate_mp_real(AV);
    matrix_t *VDq = test_mat_evaluate_mp_real(VD);

    check_bool("expr eig relation D not NULL", D != NULL);
    check_bool("expr eig relation AV not NULL", AV != NULL);
    check_bool("expr eig relation VD not NULL", VD != NULL);
    check_bool("expr eig relation AVq not NULL", AVq != NULL);
    check_bool("expr eig relation VDq not NULL", VDq != NULL);

    if (AVq && VDq) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                qfloat_t lhs;
                qfloat_t rhs;
                char label[128];

                mat_get(AVq, i, j, &lhs);
                mat_get(VDq, i, j, &rhs);
                snprintf(label, sizeof(label), "%s: AV[%zu,%zu]=VD[%zu,%zu]", label_prefix, i, j, i, j);
                check_qf_val(label, lhs, rhs, tol);
            }
        }
    }

    mat_free(D);
    mat_free(AV);
    mat_free(VD);
    mat_free(AVq);
    mat_free(VDq);
}

static void check_expr_eigenspace_relation(const char *label_prefix, const matrix_t *A, expr_t *lambda,
                                           const matrix_t *E, double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(E);
    expr_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *AE = mat_mul(A, E);
    matrix_t *ED = NULL;
    matrix_t *AEq = NULL;
    matrix_t *EDq = NULL;

    diag_vals = cols ? malloc(cols * sizeof(*diag_vals)) : NULL;
    check_bool("expr eigenspace diag alloc ok", cols == 0 || diag_vals != NULL);
    if (cols && !diag_vals)
        goto cleanup;

    for (size_t j = 0; j < cols; ++j)
        diag_vals[j] = lambda;

    D = mat_create_diagonal_expr(cols, diag_vals);
    ED = mat_mul(E, D);
    AEq = test_mat_evaluate_mp_real(AE);
    EDq = test_mat_evaluate_mp_real(ED);

    check_bool("expr eigenspace D not NULL", D != NULL);
    check_bool("expr eigenspace AE not NULL", AE != NULL);
    check_bool("expr eigenspace ED not NULL", ED != NULL);
    check_bool("expr eigenspace AEq not NULL", AEq != NULL);
    check_bool("expr eigenspace EDq not NULL", EDq != NULL);

    if (AEq && EDq) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                qfloat_t lhs;
                qfloat_t rhs;
                char label[128];

                mat_get(AEq, i, j, &lhs);
                mat_get(EDq, i, j, &rhs);
                snprintf(label, sizeof(label), "%s: AE[%zu,%zu]=LE[%zu,%zu]", label_prefix, i, j, i, j);
                check_qf_val(label, lhs, rhs, tol);
            }
        }
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(AE);
    mat_free(ED);
    mat_free(AEq);
    mat_free(EDq);
}

static void check_expr_generalized_eigenspace_relation(const char *label_prefix, const matrix_t *A, expr_t *lambda,
                                                       size_t order, const matrix_t *E, double tol)
{
    expr_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *Residual = NULL;
    matrix_t *Residual_mp_real = NULL;

    check_bool("expr generalized eigenspace input not NULL", A != NULL && lambda != NULL && E != NULL);
    if (!A || !lambda || !E)
        return;

    diag_vals = malloc(mat_get_row_count(A) * sizeof(*diag_vals));
    check_bool("expr generalized eigenspace diag alloc ok", diag_vals != NULL);
    if (!diag_vals)
        goto cleanup;

    for (size_t i = 0; i < mat_get_row_count(A); ++i)
        diag_vals[i] = lambda;

    D = mat_create_diagonal_expr(mat_get_row_count(A), diag_vals);
    Shifted = mat_sub(A, D);
    check_bool("expr generalized eigenspace diag matrix ok", D != NULL);
    check_bool("expr generalized eigenspace shifted ok", Shifted != NULL);
    if (!D || !Shifted)
        goto cleanup;

    Power = mat_pow_int(Shifted, (int)order);
    Residual = mat_mul(Power, E);
    Residual_mp_real = test_mat_evaluate_mp_real(Residual);

    check_bool("expr generalized eigenspace power not NULL", Power != NULL);
    check_bool("expr generalized eigenspace residual not NULL", Residual != NULL);
    check_bool("expr generalized eigenspace residual_mp_real not NULL", Residual_mp_real != NULL);

    if (Residual_mp_real) {
        for (size_t i = 0; i < mat_get_row_count(Residual_mp_real); ++i) {
            for (size_t j = 0; j < mat_get_col_count(Residual_mp_real); ++j) {
                qfloat_t got;
                char label[128];

                mat_get(Residual_mp_real, i, j, &got);
                snprintf(label, sizeof(label), "%s: ((A-LI)^k E)[%zu,%zu]", label_prefix, i, j);
                check_qf_val(label, got, QF_ZERO, tol);
            }
        }
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(Shifted);
    mat_free(Power);
    mat_free(Residual);
    mat_free(Residual_mp_real);
}

static matrix_t *copy_expr_column(const matrix_t *A, size_t col)
{
    matrix_t *C;

    if (!A || col >= mat_get_col_count(A))
        return NULL;

    C = mat_new_expr(mat_get_row_count(A), 1);
    if (!C)
        return NULL;

    for (size_t i = 0; i < mat_get_row_count(A); ++i) {
        expr_t *v = NULL;
        mat_get(A, i, col, &v);
        mat_set(C, i, 0, &v);
    }

    return C;
}

static void check_expr_jordan_chain_relation(const char *label_prefix, const matrix_t *A, expr_t *lambda,
                                             const matrix_t *Chain, double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(Chain);
    expr_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *Shifted = NULL;
    matrix_t *Prev = NULL;
    matrix_t *SC = NULL;
    matrix_t *SCq = NULL;
    matrix_t *Prevq = NULL;

    check_bool("expr jordan chain input not NULL", A != NULL && lambda != NULL && Chain != NULL);
    if (!A || !lambda || !Chain)
        return;

    diag_vals = malloc(rows * sizeof(*diag_vals));
    check_bool("expr jordan chain diag alloc ok", diag_vals != NULL);
    if (!diag_vals)
        goto cleanup;

    for (size_t i = 0; i < rows; ++i)
        diag_vals[i] = lambda;

    D = mat_create_diagonal_expr(rows, diag_vals);
    Shifted = mat_sub(A, D);
    check_bool("expr jordan chain D not NULL", D != NULL);
    check_bool("expr jordan chain shifted not NULL", Shifted != NULL);
    if (!D || !Shifted)
        goto cleanup;

    for (size_t j = 0; j < cols; ++j) {
        matrix_t *Col = copy_expr_column(Chain, j);
        check_bool("expr jordan chain column copy ok", Col != NULL);
        if (!Col)
            goto cleanup;

        mat_free(SC);
        mat_free(SCq);
        SC = mat_mul(Shifted, Col);
        SCq = test_mat_evaluate_mp_real(SC);
        check_bool("expr jordan chain shifted col not NULL", SC != NULL);
        check_bool("expr jordan chain shifted col qf not NULL", SCq != NULL);
        if (!SC || !SCq) {
            mat_free(Col);
            goto cleanup;
        }

        if (j == 0) {
            for (size_t i = 0; i < rows; ++i) {
                qfloat_t got;
                char label[128];

                mat_get(SCq, i, 0, &got);
                snprintf(label, sizeof(label), "%s: (A-LI)v1[%zu]", label_prefix, i);
                check_qf_val(label, got, QF_ZERO, tol);
            }
        } else {
            mat_free(Prev);
            mat_free(Prevq);
            Prev = copy_expr_column(Chain, j - 1);
            Prevq = test_mat_evaluate_mp_real(Prev);
            check_bool("expr jordan chain prev col not NULL", Prev != NULL);
            check_bool("expr jordan chain prev col qf not NULL", Prevq != NULL);
            if (!Prev || !Prevq) {
                mat_free(Col);
                goto cleanup;
            }

            for (size_t i = 0; i < rows; ++i) {
                qfloat_t got;
                qfloat_t expected;
                char label[128];

                mat_get(SCq, i, 0, &got);
                mat_get(Prevq, i, 0, &expected);
                snprintf(label, sizeof(label), "%s: (A-LI)v%zu[%zu]=v%zu[%zu]", label_prefix, j + 1, i, j, i);
                check_qf_val(label, got, expected, tol);
            }
        }

        mat_free(Col);
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(Shifted);
    mat_free(Prev);
    mat_free(SC);
    mat_free(SCq);
    mat_free(Prevq);
}

static void test_eigen_expr(void)
{
    printf(C_CYAN "TEST: eigendecomposition (expr)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(3.0, "y");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *two = test_expr_new_const_d(2.0);
        expr_t *five = test_expr_new_const_d(5.0);
        expr_t *seven = test_expr_new_const_d(7.0);
        expr_t *vals[16] = {x,         one,       two,  EXPR_ZERO, EXPR_ZERO, y,         one,       EXPR_ZERO,
                            EXPR_ZERO, EXPR_ZERO, five, one,       EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, seven};
        expr_t *ev[4] = {NULL, NULL, NULL, NULL};
        expr_t *ev2[4] = {NULL, NULL, NULL, NULL};
        matrix_t *A = mat_create_expr(4, 4, vals);
        matrix_t *V = NULL;

        print_mdv("A", A);
        check_bool("mat_eigenvalues(expr triangular) rc = 0", mat_eigenvalues_expr(A, ev) == 0);
        check_bool("expr triangular eigenvalue[0] non-null", ev[0] != NULL);
        check_bool("expr triangular eigenvalue[1] non-null", ev[1] != NULL);
        check_bool("expr triangular eigenvalue[2] non-null", ev[2] != NULL);
        check_bool("expr triangular eigenvalue[3] non-null", ev[3] != NULL);
        if (ev[0] && ev[1] && ev[2] && ev[3]) {
            check_d("expr triangular eigenvalue[0] = x", expr_eval_d(ev[0]), 2.0, 1e-12);
            check_d("expr triangular eigenvalue[1] = y", expr_eval_d(ev[1]), 3.0, 1e-12);
            check_d("expr triangular eigenvalue[2] = 5", expr_eval_d(ev[2]), 5.0, 1e-12);
            check_d("expr triangular eigenvalue[3] = 7", expr_eval_d(ev[3]), 7.0, 1e-12);
            test_expr_set_val_d(x, 11.0);
            test_expr_set_val_d(y, 13.0);
            check_d("expr triangular eigenvalue[0] tracks x", expr_eval_d(ev[0]), 11.0, 1e-12);
            check_d("expr triangular eigenvalue[1] tracks y", expr_eval_d(ev[1]), 13.0, 1e-12);
        }

        check_bool("mat_eigendecompose(expr triangular distinct) rc = 0", mat_eigendecompose_expr(A, ev2, &V) == 0);
        check_bool("expr triangular eigenvectors not NULL", V != NULL);
        if (V)
            check_expr_eigen_relation("expr triangular", A, ev2, V, 1e-12);

        for (size_t i = 0; i < 4; ++i)
            expr_free(ev[i]);
        for (size_t i = 0; i < 4; ++i)
            expr_free(ev2[i]);
        mat_free(A);
        mat_free(V);
        expr_free(x);
        expr_free(y);
        expr_free(one);
        expr_free(two);
        expr_free(five);
        expr_free(seven);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(5.0, "y");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, EXPR_ZERO, y, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, x};
        expr_t *ev[3] = {NULL, NULL, NULL};
        expr_t *ev2[3] = {NULL, NULL, NULL};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *V = NULL;

        print_mdv("A (triangular repeated expr)", A);
        check_bool("mat_eigenvalues(expr triangular repeated) rc = 0", mat_eigenvalues_expr(A, ev) == 0);
        check_bool("mat_eigendecompose(expr triangular repeated diagonalizable) rc = 0",
                   mat_eigendecompose_expr(A, ev2, &V) == 0);
        check_bool("expr triangular repeated eigenvectors not NULL", V != NULL);
        if (V)
            check_expr_eigen_relation("expr triangular repeated", A, ev2, V, 1e-12);

        for (size_t i = 0; i < 3; ++i)
            expr_free(ev[i]);
        for (size_t i = 0; i < 3; ++i)
            expr_free(ev2[i]);
        mat_free(A);
        mat_free(V);
        expr_free(x);
        expr_free(y);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, one, x};
        expr_t *ev[2] = {NULL, NULL};
        expr_t *ev2[2] = {NULL, NULL};
        matrix_t *A = mat_create_expr(2, 2, vals);
        double ev0, ev1;
        double lo, hi;
        matrix_t *V = NULL;
        matrix_t *V2 = NULL;

        print_mdv("A (dense 2x2 expr)", A);
        check_bool("mat_eigenvalues(expr dense 2x2) rc = 0", mat_eigenvalues_expr(A, ev) == 0);
        check_bool("expr dense eigenvalue[0] non-null", ev[0] != NULL);
        check_bool("expr dense eigenvalue[1] non-null", ev[1] != NULL);
        if (ev[0] && ev[1]) {
            ev0 = expr_eval_d(ev[0]);
            ev1 = expr_eval_d(ev[1]);
            lo = fmin(ev0, ev1);
            hi = fmax(ev0, ev1);
            check_d("expr dense 2x2 eigenvalue min = x-1", lo, 2.0, 1e-12);
            check_d("expr dense 2x2 eigenvalue max = x+1", hi, 4.0, 1e-12);

            test_expr_set_val_d(x, 10.0);
            ev0 = expr_eval_d(ev[0]);
            ev1 = expr_eval_d(ev[1]);
            lo = fmin(ev0, ev1);
            hi = fmax(ev0, ev1);
            check_d("expr dense 2x2 eigenvalue min tracks x", lo, 9.0, 1e-12);
            check_d("expr dense 2x2 eigenvalue max tracks x", hi, 11.0, 1e-12);
        }

        check_bool("mat_eigendecompose(expr dense 2x2) rc = 0", mat_eigendecompose_expr(A, ev2, &V) == 0);
        check_bool("expr dense 2x2 eigenvectors not NULL", V != NULL);
        if (V)
            check_expr_eigen_relation("expr dense 2x2", A, ev2, V, 1e-20);

        V2 = mat_eigenvectors(A);
        check_bool("mat_eigenvectors(expr dense 2x2) not NULL", V2 != NULL);

        expr_free(ev[0]);
        expr_free(ev[1]);
        expr_free(ev2[0]);
        expr_free(ev2[1]);
        mat_free(A);
        mat_free(V);
        mat_free(V2);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, EXPR_ZERO, x};
        expr_t *ev[2] = {NULL, NULL};
        expr_t *ev2[2] = {NULL, NULL};
        matrix_t *A = mat_create_expr(2, 2, vals);
        matrix_t *V = NULL;

        check_bool("mat_eigenvalues(expr Jordan 2x2) rc = 0", mat_eigenvalues_expr(A, ev) == 0);
        check_bool("mat_eigendecompose(expr Jordan 2x2) remains unsupported",
                   mat_eigendecompose_expr(A, ev2, &V) < 0 && V == NULL);

        expr_free(ev[0]);
        expr_free(ev[1]);
        expr_free(ev2[0]);
        expr_free(ev2[1]);
        mat_free(A);
        expr_free(x);
        expr_free(one);
    }
}

static void test_eigenspace_expr(void)
{
    printf(C_CYAN "TEST: eigenspace (expr)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(5.0, "y");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, EXPR_ZERO, y, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *E = mat_eigenspace(A, &x);

        print_mdv("A (eigenspace repeated expr)", A);
        check_bool("mat_eigenspace(expr repeated triangular) not NULL", E != NULL);
        if (E) {
            print_mdv("eigenspace_x(A)", E);
            check_bool("eigenspace rows = 3", mat_get_row_count(E) == 3);
            check_bool("eigenspace cols = 2", mat_get_col_count(E) == 2);
            check_expr_eigenspace_relation("expr repeated eigenspace", A, x, E, 1e-20);
            test_expr_set_val_d(x, 11.0);
            test_expr_set_val_d(y, 13.0);
            check_expr_eigenspace_relation("expr repeated eigenspace tracks", A, x, E, 1e-20);
        }

        mat_free(A);
        mat_free(E);
        expr_free(x);
        expr_free(y);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(2, 2, vals);
        matrix_t *E = mat_eigenspace(A, &x);

        print_mdv("A (Jordan eigenspace expr)", A);
        check_bool("mat_eigenspace(expr Jordan 2x2) not NULL", E != NULL);
        if (E) {
            print_mdv("eigenspace_x(Jordan A)", E);
            check_bool("Jordan eigenspace rows = 2", mat_get_row_count(E) == 2);
            check_bool("Jordan eigenspace cols = 1", mat_get_col_count(E) == 1);
            check_expr_eigenspace_relation("expr Jordan eigenspace", A, x, E, 1e-20);
            test_expr_set_val_d(x, 9.0);
            check_expr_eigenspace_relation("expr Jordan eigenspace tracks", A, x, E, 1e-20);
        }

        mat_free(A);
        mat_free(E);
        expr_free(x);
        expr_free(one);
    }
}

static void test_generalized_eigenspace_expr(void)
{
    printf(C_CYAN "TEST: generalized eigenspace (expr)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(2, 2, vals);
        matrix_t *G = mat_generalized_eigenspace(A, &x, 2);

        print_mdv("A (Jordan generalized eigenspace expr)", A);
        check_bool("mat_generalized_eigenspace(expr Jordan 2x2,2) not NULL", G != NULL);
        if (G) {
            print_mdv("gen_eigenspace_x^2(A)", G);
            check_bool("Jordan generalized eigenspace rows = 2", mat_get_row_count(G) == 2);
            check_bool("Jordan generalized eigenspace cols = 2", mat_get_col_count(G) == 2);
            check_expr_generalized_eigenspace_relation("expr Jordan generalized eigenspace", A, x, 2, G, 1e-20);
            test_expr_set_val_d(x, 9.0);
            check_expr_generalized_eigenspace_relation("expr Jordan generalized eigenspace tracks", A, x, 2, G, 1e-20);
        }

        mat_free(A);
        mat_free(G);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, EXPR_ZERO, x, one, EXPR_ZERO, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *G2 = mat_generalized_eigenspace(A, &x, 2);
        matrix_t *G3 = mat_generalized_eigenspace(A, &x, 3);

        print_mdv("A (3x3 Jordan generalized eigenspace expr)", A);
        check_bool("mat_generalized_eigenspace(expr Jordan 3x3,2) not NULL", G2 != NULL);
        check_bool("mat_generalized_eigenspace(expr Jordan 3x3,3) not NULL", G3 != NULL);
        if (G2) {
            print_mdv("gen_eigenspace_x^2(A)", G2);
            check_bool("3x3 Jordan generalized eigenspace order-2 rows = 3", mat_get_row_count(G2) == 3);
            check_bool("3x3 Jordan generalized eigenspace order-2 cols = 2", mat_get_col_count(G2) == 2);
            check_expr_generalized_eigenspace_relation("expr 3x3 Jordan generalized eigenspace order-2", A, x, 2, G2,
                                                       1e-20);
        }
        if (G3) {
            print_mdv("gen_eigenspace_x^3(A)", G3);
            check_bool("3x3 Jordan generalized eigenspace order-3 rows = 3", mat_get_row_count(G3) == 3);
            check_bool("3x3 Jordan generalized eigenspace order-3 cols = 3", mat_get_col_count(G3) == 3);
            check_expr_generalized_eigenspace_relation("expr 3x3 Jordan generalized eigenspace order-3", A, x, 3, G3,
                                                       1e-20);
            test_expr_set_val_d(x, 5.0);
            check_expr_generalized_eigenspace_relation("expr 3x3 Jordan generalized eigenspace order-3 tracks", A, x, 3,
                                                       G3, 1e-20);
        }

        mat_free(A);
        mat_free(G2);
        mat_free(G3);
        expr_free(x);
        expr_free(one);
    }
}

static void test_jordan_chain_expr(void)
{
    printf(C_CYAN "TEST: Jordan chain (expr)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(2, 2, vals);
        matrix_t *J = mat_jordan_chain(A, &x, 2);

        print_mdv("A (Jordan chain 2x2 expr)", A);
        check_bool("mat_jordan_chain(expr Jordan 2x2,2) not NULL", J != NULL);
        if (J) {
            print_mdv("jordan_chain_x^2(A)", J);
            check_bool("Jordan chain 2x2 rows = 2", mat_get_row_count(J) == 2);
            check_bool("Jordan chain 2x2 cols = 2", mat_get_col_count(J) == 2);
            check_expr_jordan_chain_relation("expr Jordan chain 2x2", A, x, J, 1e-20);
            test_expr_set_val_d(x, 9.0);
            check_expr_jordan_chain_relation("expr Jordan chain 2x2 tracks", A, x, J, 1e-20);
        }

        mat_free(A);
        mat_free(J);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, EXPR_ZERO, x, one, EXPR_ZERO, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *J = mat_jordan_chain(A, &x, 3);

        print_mdv("A (Jordan chain 3x3 expr)", A);
        check_bool("mat_jordan_chain(expr Jordan 3x3,3) not NULL", J != NULL);
        if (J) {
            print_mdv("jordan_chain_x^3(A)", J);
            check_bool("Jordan chain 3x3 rows = 3", mat_get_row_count(J) == 3);
            check_bool("Jordan chain 3x3 cols = 3", mat_get_col_count(J) == 3);
            check_expr_jordan_chain_relation("expr Jordan chain 3x3", A, x, J, 1e-20);
            test_expr_set_val_d(x, 5.0);
            check_expr_jordan_chain_relation("expr Jordan chain 3x3 tracks", A, x, J, 1e-20);
        }

        mat_free(A);
        mat_free(J);
        expr_free(x);
        expr_free(one);
    }
}

static void test_jordan_profile_expr(void)
{
    printf(C_CYAN "TEST: Jordan profile (expr)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[4] = {x, one, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(2, 2, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile 2x2 expr)", A);
        check_bool("mat_jordan_profile(expr Jordan 2x2) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile 2x2 rows = 1", mat_get_row_count(P) == 1);
            check_bool("Jordan profile 2x2 cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            check_d("Jordan profile 2x2[0] = 2", p0, 2.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(5.0, "y");
        expr_t *vals[9] = {x, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, x, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, y};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile repeated diagonal expr)", A);
        check_bool("mat_jordan_profile(repeated diagonal expr) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            double p1 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile repeated diagonal rows = 2", mat_get_row_count(P) == 2);
            check_bool("Jordan profile repeated diagonal cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            mat_get(P, 1, 0, &p1);
            check_d("Jordan profile repeated diagonal[0] = 1", p0, 1.0, 1e-12);
            check_d("Jordan profile repeated diagonal[1] = 1", p1, 1.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        expr_free(x);
        expr_free(y);
    }

    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, EXPR_ZERO, x, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, x};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile mixed 3x3 expr)", A);
        check_bool("mat_jordan_profile(mixed 3x3 expr) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            double p1 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile mixed 3x3 rows = 2", mat_get_row_count(P) == 2);
            check_bool("Jordan profile mixed 3x3 cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            mat_get(P, 1, 0, &p1);
            check_d("Jordan profile mixed 3x3[0] = 2", p0, 2.0, 1e-12);
            check_d("Jordan profile mixed 3x3[1] = 1", p1, 1.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        expr_free(x);
        expr_free(one);
    }
}

/* ------------------------------------------------------------------ mat_exp */

static void test_mat_exp_d(void)
{
    printf(C_CYAN "TEST: mat_exp (double)\n" C_RESET);

    /* 2×2 diagonal: exp(diag(a,b)) = diag(exp(a),exp(b)) */
    {
        double A_vals[4] = {1.0, 0.0, 0.0, 2.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(diag) not NULL", E != NULL);
        if (E) {
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(sym) not NULL", E != NULL);
        if (E) {
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(zero) not NULL", E != NULL);
        if (E) {
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

static void test_mat_exp_mp_real(void)
{
    printf(C_CYAN "TEST: mat_exp (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        number_t A_vals[4] = {NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, A_vals);
        print_mnum("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(sym) not NULL", E != NULL);
        if (E) {
            print_mnum("exp(A)", E);
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

static void test_mat_exp_complex(void)
{
    printf(C_CYAN "TEST: mat_exp (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        number_t A_vals[4] = {NUM_ZERO, num_create_from_string("i"), num_create_from_string("-i"), NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, A_vals);
        print_mnum("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qc(herm) not NULL", E != NULL);
        if (E) {
            print_mnum("exp(A)", E);
            number_t e00 = mat_get_num(E, 0, 0);
            number_t e01 = mat_get_num(E, 0, 1);
            number_t e10 = mat_get_num(E, 1, 0);
            number_t e11 = mat_get_num(E, 1, 1);
            number_t ch = num_cosh(NUM_ONE);
            number_t sh = num_sinh(NUM_ONE);
            number_t ish = num_mul(NUM_I, sh);
            number_t nish = num_neg(ish);
            check_num_close_local("qc exp(herm)[0,0] = cosh(1)", e00, ch, 1e-25);
            check_num_close_local("qc exp(herm)[1,1] = cosh(1)", e11, ch, 1e-25);
            check_num_close_local("qc exp(herm)[0,1] = i·sinh(1)", e01, ish, 1e-25);
            check_num_close_local("qc exp(herm)[1,0] = -i·sinh(1)", e10, nish, 1e-25);
            num_destroy(&nish);
            num_destroy(&ish);
            num_destroy(&sh);
            num_destroy(&ch);
            num_destroy(&e11);
            num_destroy(&e10);
            num_destroy(&e01);
            num_destroy(&e00);
        }
        num_destroy(&A_vals[1]);
        num_destroy(&A_vals[2]);
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_singular(void)
{
    printf(C_CYAN "TEST: mat_exp on singular square matrices\n" C_RESET);

    /* singular diagonal: exp(diag(0,2)) = diag(1,e^2) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 2.0};
        double expected_vals[4] = {1.0, 0.0, 0.0, exp(2.0)};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        matrix_t *E_expected = test_mat_create_d(2, 2, expected_vals);
        check_bool("singular diagonal allocated", A != NULL);
        check_bool("singular diagonal expected allocated", E_expected != NULL);
        if (!A || !E_expected) {
            mat_free(A);
            mat_free(E_expected);
            return;
        }

        print_md("A (singular diagonal)", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(singular diagonal) not NULL", E != NULL);
        if (E) {
            check_bool("exp(diag(0,2)) preserves diagonal structure", mat_is_diagonal(E));
            if (!test_assert_matrix_d_close(E, E_expected, 1e-12, __FILE__, __LINE__)) {
                mat_free(A);
                mat_free(E);
                mat_free(E_expected);
                return;
            }
        }

        mat_free(A);
        mat_free(E);
        mat_free(E_expected);
    }

    /* singular nilpotent Jordan block: exp(N) = I + N */
    {
        double N_vals[4] = {0.0, 1.0, 0.0, 0.0};
        double expected_vals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *N = test_mat_create_d(2, 2, N_vals);
        matrix_t *E_expected = test_mat_create_d(2, 2, expected_vals);
        check_bool("singular nilpotent allocated", N != NULL);
        check_bool("singular nilpotent expected allocated", E_expected != NULL);
        if (!N || !E_expected) {
            mat_free(N);
            mat_free(E_expected);
            return;
        }

        print_md("N (singular nilpotent)", N);
        matrix_t *E = mat_exp(N);
        check_bool("mat_exp(singular nilpotent) not NULL", E != NULL);
        if (E) {
            check_bool("exp(N) preserves upper-triangular structure", mat_is_upper_triangular(E));
            if (!test_assert_matrix_d_close(E, E_expected, 1e-12, __FILE__, __LINE__)) {
                mat_free(N);
                mat_free(E);
                mat_free(E_expected);
                return;
            }
        }

        mat_free(N);
        mat_free(E);
        mat_free(E_expected);
    }
}

static void test_matrix_function_structure_preservation(void)
{
    printf(C_CYAN "TEST: matrix functions preserve structured layouts when possible\n" C_RESET);

    {
        double A_vals[4] = {2.0, 0.0, 0.0, 3.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        matrix_t *L = mat_log(A);
        matrix_t *Ln = mat_ln(A);
        matrix_t *Lg = mat_lg(A);
        matrix_t *Log10 = mat_log10(A);
        matrix_t *E = NULL;

        check_bool("positive diagonal input allocated", A != NULL);
        check_bool("mat_log(positive diagonal) not NULL", L != NULL);
        check_bool("mat_ln aliases mat_log", L && Ln && test_assert_matrix_d_close(Ln, L, 1e-12, __FILE__, __LINE__));
        check_bool("mat_lg aliases mat_log10",
                   Lg && Log10 && test_assert_matrix_d_close(Lg, Log10, 1e-12, __FILE__, __LINE__));
        if (L) {
            check_bool("mat_log(positive diagonal) preserves diagonal structure", mat_is_diagonal(L));
            E = mat_exp(L);
            check_bool("mat_exp(mat_log(positive diagonal)) not NULL", E != NULL);
            if (E) {
                check_bool("mat_exp(mat_log(positive diagonal)) preserves diagonal structure", mat_is_diagonal(E));
                if (!test_assert_matrix_d_close(E, A, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(Log10);
                    mat_free(Lg);
                    mat_free(Ln);
                    mat_free(L);
                    mat_free(A);
                    return;
                }
            }
        }

        mat_free(E);
        mat_free(Log10);
        mat_free(Lg);
        mat_free(Ln);
        mat_free(L);
        mat_free(A);
    }

    {
        double A_vals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        matrix_t *L = mat_log(A);
        matrix_t *S = mat_sqrt(A);
        matrix_t *E = NULL;

        check_bool("upper-triangular Jordan input allocated", A != NULL);
        check_bool("mat_log(upper-triangular Jordan) not NULL", L != NULL);
        check_bool("mat_sqrt(upper-triangular Jordan) not NULL", S != NULL);
        if (L) {
            check_bool("mat_log(upper-triangular Jordan) preserves upper-triangular structure",
                       mat_is_upper_triangular(L));
            E = mat_exp(L);
            check_bool("mat_exp(mat_log(upper-triangular Jordan)) not NULL", E != NULL);
            if (E) {
                check_bool("mat_exp(mat_log(upper-triangular Jordan)) preserves upper-triangular structure",
                           mat_is_upper_triangular(E));
                if (!test_assert_matrix_d_close(E, A, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(L);
                    mat_free(S);
                    mat_free(A);
                    return;
                }
            }
        }
        if (S) {
            check_bool("mat_sqrt(upper-triangular Jordan) preserves upper-triangular structure",
                       mat_is_upper_triangular(S));
        }

        mat_free(E);
        mat_free(L);
        mat_free(S);
        mat_free(A);
    }
}

static void test_mat_fun_singular_entire_d(void)
{
    printf(C_CYAN "TEST: entire matrix functions on singular square matrices\n" C_RESET);

    /* For the nilpotent Jordan block N with N^2 = 0, any entire function
     * satisfies f(N) = f(0) I + f'(0) N. */
    {
        double N_vals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = test_mat_create_d(2, 2, N_vals);
        check_bool("nilpotent singular test matrix allocated", N != NULL);
        if (!N)
            return;

        print_md("N (nilpotent singular)", N);

        {
            matrix_t *R = mat_sin(N);
            check_bool("mat_sin(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_cos(N);
            check_bool("mat_cos(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, 0.0, 0.0, 1.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_sinh(N);
            check_bool("mat_sinh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_cosh(N);
            check_bool("mat_cosh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, 0.0, 0.0, 1.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_tan(N);
            check_bool("mat_tan(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_tanh(N);
            check_bool("mat_tanh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_erf(N);
            double c = 2.0 / sqrt(M_PI);
            check_bool("mat_erf(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, c, 0.0, 0.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_erfc(N);
            double c = 2.0 / sqrt(M_PI);
            check_bool("mat_erfc(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, -c, 0.0, 1.0};
                matrix_t *E = test_mat_create_d(2, 2, expected_vals);
                if (!test_assert_matrix_d_close(R, E, 1e-12, __FILE__, __LINE__)) {
                    mat_free(E);
                    mat_free(R);
                    mat_free(N);
                    return;
                }
                mat_free(E);
            }
            mat_free(R);
        }

        mat_free(N);
    }
}

static void test_mat_exp_null_safety(void)
{
    printf(C_CYAN "TEST: mat_exp null safety\n" C_RESET);
    check_bool("mat_exp(NULL) = NULL", mat_exp(NULL) == NULL);

    matrix_t *A = test_mat_dense_d(2, 3);
    check_bool("mat_exp(non-square) = NULL", mat_exp(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sin */

static void test_mat_sin_d(void)
{
    printf(C_CYAN "TEST: mat_sin (double)\n" C_RESET);

    /* 2×2 diagonal: sin(diag(0, π/2)) = diag(0, 1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI / 2.0};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(diag) not NULL", S != NULL);
        if (S) {
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(sym) not NULL", S != NULL);
        if (S) {
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(zero) not NULL", S != NULL);
        if (S) {
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

static void test_mat_sin_mp_real(void)
{
    printf(C_CYAN "TEST: mat_sin (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → sin(A) = sin(1)·A */
    {
        number_t A_vals[4] = {NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, A_vals);
        print_mnum("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(sym) not NULL", S != NULL);
        if (S) {
            print_mnum("sin(A)", S);
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

static void test_mat_sin_complex(void)
{
    printf(C_CYAN "TEST: mat_sin (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A = [[0, i·sin(1)], [-i·sin(1), 0]] */
    {
        number_t A_vals[4] = {NUM_ZERO, num_create_from_string("i"), num_create_from_string("-i"), NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, A_vals);
        print_mnum("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qc(herm) not NULL", S != NULL);
        if (S) {
            print_mnum("sin(A)", S);
            number_t s00 = mat_get_num(S, 0, 0);
            number_t s01 = mat_get_num(S, 0, 1);
            number_t s10 = mat_get_num(S, 1, 0);
            number_t s11 = mat_get_num(S, 1, 1);
            number_t s1 = num_sin(NUM_ONE);
            number_t ish = num_mul(NUM_I, s1);
            number_t nish = num_neg(ish);
            check_num_close_local("qc sin(herm)[0,0] = 0", s00, NUM_ZERO, 1e-25);
            check_num_close_local("qc sin(herm)[1,1] = 0", s11, NUM_ZERO, 1e-25);
            check_num_close_local("qc sin(herm)[0,1] = i·sin(1)", s01, ish, 1e-25);
            check_num_close_local("qc sin(herm)[1,0] = -i·sin(1)", s10, nish, 1e-25);
            num_destroy(&nish);
            num_destroy(&ish);
            num_destroy(&s1);
            num_destroy(&s11);
            num_destroy(&s10);
            num_destroy(&s01);
            num_destroy(&s00);
        }
        num_destroy(&A_vals[1]);
        num_destroy(&A_vals[2]);
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sin null safety\n" C_RESET);
    check_bool("mat_sin(NULL) = NULL", mat_sin(NULL) == NULL);

    matrix_t *A = test_mat_dense_d(2, 3);
    check_bool("mat_sin(non-square) = NULL", mat_sin(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_cos */

static void test_mat_cos_d(void)
{
    printf(C_CYAN "TEST: mat_cos (double)\n" C_RESET);

    /* 2×2 diagonal: cos(diag(0, π)) = diag(1, -1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI};
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(diag) not NULL", C != NULL);
        if (C) {
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
        matrix_t *A = test_mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(sym) not NULL", C != NULL);
        if (C) {
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

static void test_mat_cos_mp_real(void)
{
    printf(C_CYAN "TEST: mat_cos (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → cos(A) = cos(1)·I */
    {
        number_t vals[4] = {NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, vals);
        print_mnum("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(sym) not NULL", C != NULL);
        if (C) {
            print_mnum("cos(A)", C);
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

static void test_mat_cos_complex(void)
{
    printf(C_CYAN "TEST: mat_cos (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        number_t A_vals[4] = {NUM_ZERO, num_create_from_string("i"), num_create_from_string("-i"), NUM_ZERO};
        matrix_t *A = mat_create_num(2, 2, A_vals);
        print_mnum("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qc(herm) not NULL", C != NULL);
        if (C) {
            print_mnum("cos(A)", C);
            number_t c00 = mat_get_num(C, 0, 0);
            number_t c01 = mat_get_num(C, 0, 1);
            number_t c10 = mat_get_num(C, 1, 0);
            number_t c11 = mat_get_num(C, 1, 1);
            number_t c1 = num_cos(NUM_ONE);
            check_num_close_local("qc cos(herm)[0,0] = cos(1)", c00, c1, 1e-25);
            check_num_close_local("qc cos(herm)[1,1] = cos(1)", c11, c1, 1e-25);
            check_num_close_local("qc cos(herm)[0,1] = 0", c01, NUM_ZERO, 1e-25);
            check_num_close_local("qc cos(herm)[1,0] = 0", c10, NUM_ZERO, 1e-25);
            num_destroy(&c1);
            num_destroy(&c11);
            num_destroy(&c10);
            num_destroy(&c01);
            num_destroy(&c00);
        }
        num_destroy(&A_vals[1]);
        num_destroy(&A_vals[2]);
        mat_free(A);
        mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tan */

static void test_mat_tan_d(void)
{
    printf(C_CYAN "TEST: mat_tan (double)\n" C_RESET);

    /* 2×2 diagonal: tan(diag(0, π/4)) = diag(0, 1) */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, h = M_PI / 4.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &h);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(diag) not NULL", T != NULL);
        if (T) {
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
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(sym) not NULL", T != NULL);
        if (T) {
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

    /* 2×2 diagonal: sinh(diag(0, 1)) = diag(0, sinh(1)) */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(diag) not NULL", S != NULL);
        if (S) {
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
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(sym) not NULL", S != NULL);
        if (S) {
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

    /* 2×2 diagonal: cosh(diag(0, 1)) = diag(1, cosh(1)) */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(diag) not NULL", C != NULL);
        if (C) {
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
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(sym) not NULL", C != NULL);
        if (C) {
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

    /* 2×2 diagonal: tanh(diag(0, 1)) = diag(0, tanh(1)) */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(diag) not NULL", T != NULL);
        if (T) {
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
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(sym) not NULL", T != NULL);
        if (T) {
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

    matrix_t *A = test_mat_dense_d(2, 3);
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

    /* 2×2 diagonal: sqrt(diag(1,4)) = diag(1,2) */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, o = 1.0, f = 4.0;
        mat_set(A, 0, 0, &o);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &f);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(diag) not NULL", S != NULL);
        if (S) {
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
        matrix_t *A = test_mat_dense_d(2, 2);
        double o = 1.0, t = 2.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &t);
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

static void test_mat_sqrt_mp_real(void)
{
    printf(C_CYAN "TEST: mat_sqrt (qfloat)\n" C_RESET);

    /* 2×2 diagonal: sqrt(diag(1,9)) = diag(1,3) */
    {
        number_t vals[4] = {NUM_ONE, NUM_ZERO, NUM_ZERO, num_create_from_double(9.0)};
        matrix_t *A = mat_create_num(2, 2, vals);
        print_mnum("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("qf mat_sqrt(diag) not NULL", S != NULL);
        if (S) {
            print_mnum("sqrt(A)", S);
            qfloat_t s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_qf_val("qf sqrt(diag)[0,0] = 1", s00, QF_ONE, 1e-25);
            check_qf_val("qf sqrt(diag)[0,1] = 0", s01, QF_ZERO, 1e-25);
            check_qf_val("qf sqrt(diag)[1,0] = 0", s10, QF_ZERO, 1e-25);
            check_qf_val("qf sqrt(diag)[1,1] = 3", s11, qf_from_double(3.0), 1e-25);
        }
        num_destroy(&vals[3]);
        mat_free(A);
        mat_free(S);
    }
}

/* ------------------------------------------------------------------ mat_log */

static void test_mat_log_d(void)
{
    printf(C_CYAN "TEST: mat_log (double)\n" C_RESET);

    /* exp(log(A)) = A for diagonal [[2,0],[0,3]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, t = 2.0, th = 3.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &th);
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

    /* sin(asin(A)) = A for 2×2 symmetric with small eigenvalues */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double v00 = 0.3, v01 = 0.1, v10 = 0.1, v11 = 0.2;
        mat_set(A, 0, 0, &v00);
        mat_set(A, 0, 1, &v01);
        mat_set(A, 1, 0, &v10);
        mat_set(A, 1, 1, &v11);
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

    /* asin(A) + acos(A) = (π/2)·I for diagonal [[0.3, 0], [0, 0.4]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, v0 = 0.3, v1 = 0.4;
        mat_set(A, 0, 0, &v0);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &v1);
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

    /* tan(atan(A)) = A for small diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
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

    /* sinh(asinh(A)) = A for diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
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

    /* cosh(acosh(A)) = A for diagonal [[2, 0],[0, 3]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, a = 2.0, b = 3.0;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
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

    /* tanh(atanh(A)) = A for diagonal [[0.4, 0],[0, 0.2]] */
    {
        matrix_t *A = test_mat_dense_d(2, 2);
        double z = 0.0, a = 0.4, b = 0.2;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
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

    matrix_t *A = test_mat_dense_d(2, 3);
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

    /* A = [[3, 1], [0, 2]] — upper triangular, eigenvalues 3 and 2.
     * eigenvector for λ=3: [1, 0]
     * eigenvector for λ=2: [-1, 1] (normalised) */
    matrix_t *A = test_mat_dense_d(2, 2);
    double a00 = 3.0, a01 = 1.0, a10 = 0.0, a11 = 2.0;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);
    print_md("A", A);

    number_t eigenvalues[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("mat_eigendecompose_general: rc = 0", rc == 0);
    check_bool("mat_eigendecompose_general: V not NULL", V != NULL);
    check_bool("mat_eigendecompose_general: V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (V) {
        matrix_t *Aq = test_mat_evaluate_complex(A);
        matrix_t *Vq = test_mat_evaluate_complex(V);

        print_mnum("V (eigenvectors)", V);
        /* Verify A·v_j = lambda_j · v_j for each column */
        for (int j = 0; Aq && Vq && j < 2; j++) {
            qcomplex_t lam = num_to_qcomplex(eigenvalues[j]);
            qcomplex_t v0, v1;
            mat_get(Vq, 0, j, &v0);
            mat_get(Vq, 1, j, &v1);
            /* [Av]_0 = 3*v0 + 1*v1, [Av]_1 = 2*v1 */
            qcomplex_t av0 = qc_add(qc_mul(qc_make(qf_from_double(3.0), QF_ZERO), v0), v1);
            qcomplex_t av1 = qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), v1);
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "(Av)[0,%d] = lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "(Av)[1,%d] = lam*v[1,%d]", j, j);
            check_qc_val(label0, av0, qc_mul(lam, v0), 1e-10);
            check_qc_val(label1, av1, qc_mul(lam, v1), 1e-10);
        }
        mat_free(Aq);
        mat_free(Vq);
        mat_free(V);
    }
    mat_free(A);
    num_destroy(&eigenvalues[0]);
    num_destroy(&eigenvalues[1]);

    /* Dense non-Hermitian case:
     *   P = [[1,1],[1,2]], D = diag(3,2), A = P D P^-1 = [[4,-1],[2,1]]
     * so the eigenpairs are exact but the input is no longer triangular. */
    {
        double avals[4] = {4.0, -1.0, 2.0, 1.0};
        matrix_t *B = test_mat_create_d(2, 2, avals);
        number_t evals[2] = {NUM_ZERO, NUM_ZERO};
        matrix_t *W = NULL;

        check_bool("dense non-Hermitian B allocated", B != NULL);
        if (!B)
            return;

        print_md("B (dense non-Hermitian)", B);

        int rc2 = mat_eigendecompose(B, evals, &W);
        check_bool("dense non-Hermitian eigendecompose rc = 0", rc2 == 0);
        check_bool("dense non-Hermitian eigenvectors not NULL", W != NULL);

        if (W) {
            matrix_t *Wq = test_mat_evaluate_complex(W);
            print_mnum("W (eigenvectors)", W);
            for (int j = 0; Wq && j < 2; j++) {
                qcomplex_t lam = num_to_qcomplex(evals[j]);
                qcomplex_t w0, w1;
                qcomplex_t bw0, bw1;
                char label0[80], label1[80];

                mat_get(Wq, 0, j, &w0);
                mat_get(Wq, 1, j, &w1);
                bw0 = qc_add(qc_mul(qc_make(qf_from_double(4.0), QF_ZERO), w0),
                             qc_mul(qc_make(qf_from_double(-1.0), QF_ZERO), w1));
                bw1 = qc_add(qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), w0), w1);

                snprintf(label0, sizeof(label0), "(Bw)[0,%d] = lam*w[0,%d]", j, j);
                snprintf(label1, sizeof(label1), "(Bw)[1,%d] = lam*w[1,%d]", j, j);
                check_qc_val(label0, bw0, qc_mul(lam, w0), 1e-10);
                check_qc_val(label1, bw1, qc_mul(lam, w1), 1e-10);
            }

            number_t eval0_re = num_real_part(evals[0]);
            number_t eval1_re = num_real_part(evals[1]);
            double ev_min = fmin(num_to_double(eval0_re), num_to_double(eval1_re));
            double ev_max = fmax(num_to_double(eval0_re), num_to_double(eval1_re));
            check_d("dense non-Hermitian eigenvalue min = 2", ev_min, 2.0, 1e-10);
            check_d("dense non-Hermitian eigenvalue max = 3", ev_max, 3.0, 1e-10);
            num_destroy(&eval1_re);
            num_destroy(&eval0_re);
            mat_free(Wq);
            mat_free(W);
        }

        num_destroy(&evals[0]);
        num_destroy(&evals[1]);
        mat_free(B);
    }
}

static void test_eigen_general_mp_real(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, qfloat)\n" C_RESET);

    /* A = [[4, 1], [0, 1]] — eigenvalues 1 and 4 */
    number_t vals[4] = {num_create_from_double(4.0), NUM_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *A = mat_create_num(2, 2, vals);
    print_mnum("A", A);

    number_t eigenvalues[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("qf eigendecompose_general: rc = 0", rc == 0);
    check_bool("qf eigendecompose_general: V not NULL", V != NULL);
    check_bool("qf eigendecompose_general: V type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (V) {
        matrix_t *Aq = test_mat_evaluate_complex(A);
        matrix_t *Vq = test_mat_evaluate_complex(V);
        print_mnum("V", V);
        for (int j = 0; Aq && Vq && j < 2; j++) {
            qcomplex_t lam = num_to_qcomplex(eigenvalues[j]);
            qcomplex_t v0, v1;
            mat_get(Vq, 0, j, &v0);
            mat_get(Vq, 1, j, &v1);
            qcomplex_t av0 =
                qc_add(qc_mul(qc_make(qf_from_double(4.0), QF_ZERO), v0), qc_mul(qc_make(QF_ONE, QF_ZERO), v1));
            qcomplex_t av1 = qc_mul(qc_make(QF_ONE, QF_ZERO), v1);
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "qf (Av)[0,%d]=lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "qf (Av)[1,%d]=lam*v[1,%d]", j, j);
            check_qc_val(label0, av0, qc_mul(lam, v0), 1e-14);
            check_qc_val(label1, av1, qc_mul(lam, v1), 1e-14);
        }
        mat_free(Aq);
        mat_free(Vq);
        mat_free(V);
    }
    mat_free(A);
    num_destroy(&eigenvalues[0]);
    num_destroy(&eigenvalues[1]);

    /* Same dense non-Hermitian similarity transform as the double test:
     * A = [[4,-1],[2,1]] has eigenvalues 3 and 2 but is not triangular. */
    {
        number_t avals[4] = {num_create_from_double(4.0), num_create_from_double(-1.0), num_create_from_double(2.0),
                             num_create_from_double(1.0)};
        matrix_t *B = mat_create_num(2, 2, avals);
        number_t evals[2] = {NUM_ZERO, NUM_ZERO};
        matrix_t *W = NULL;
        for (size_t i = 0; i < 4u; ++i)
            num_destroy(&avals[i]);

        check_bool("qf dense non-Hermitian B allocated", B != NULL);
        if (!B)
            return;

        print_mnum("B (dense non-Hermitian)", B);

        int rc2 = mat_eigendecompose(B, evals, &W);
        check_bool("qf dense non-Hermitian eigendecompose rc = 0", rc2 == 0);
        check_bool("qf dense non-Hermitian eigenvectors not NULL", W != NULL);

        if (W) {
            qfloat_t four = qf_from_double(4.0);
            qfloat_t minus_one = qf_from_double(-1.0);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t one = QF_ONE;
            matrix_t *Wq = test_mat_evaluate_complex(W);
            print_mnum("W", W);
            for (int j = 0; Wq && j < 2; j++) {
                qcomplex_t lam = num_to_qcomplex(evals[j]);
                qcomplex_t w0, w1;
                qcomplex_t bw0, bw1;
                char label0[96], label1[96];

                mat_get(Wq, 0, j, &w0);
                mat_get(Wq, 1, j, &w1);
                bw0 = qc_add(qc_mul(qc_make(four, QF_ZERO), w0), qc_mul(qc_make(minus_one, QF_ZERO), w1));
                bw1 = qc_add(qc_mul(qc_make(two, QF_ZERO), w0), qc_mul(qc_make(one, QF_ZERO), w1));

                snprintf(label0, sizeof(label0), "qf (Bw)[0,%d]=lam*w[0,%d]", j, j);
                snprintf(label1, sizeof(label1), "qf (Bw)[1,%d]=lam*w[1,%d]", j, j);
                check_qc_val(label0, bw0, qc_mul(lam, w0), 1e-14);
                check_qc_val(label1, bw1, qc_mul(lam, w1), 1e-14);
            }

            number_t eval0_re = num_real_part(evals[0]);
            number_t eval1_re = num_real_part(evals[1]);
            int e0_smaller = num_lt(eval0_re, eval1_re);
            number_t ev_min = e0_smaller ? num_clone(eval0_re) : num_clone(eval1_re);
            number_t ev_max = e0_smaller ? num_clone(eval1_re) : num_clone(eval0_re);
            check_qf_val("qf dense non-Hermitian eigenvalue min = 2", num_to_qfloat(ev_min), qf_from_double(2.0),
                         1e-25);
            check_qf_val("qf dense non-Hermitian eigenvalue max = 3", num_to_qfloat(ev_max), qf_from_double(3.0),
                         1e-25);
            num_destroy(&ev_min);
            num_destroy(&ev_max);
            num_destroy(&eval1_re);
            num_destroy(&eval0_re);
            mat_free(Wq);
            mat_free(W);
        }

        num_destroy(&evals[0]);
        num_destroy(&evals[1]);
        mat_free(B);
    }
}

static void test_eigen_general_num_high_precision(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, number high precision)\n" C_RESET);

    number_t A_vals[4] = {num_create_from_string("5.6"), num_create_from_string("-1.8"), num_create_from_string("1.2"),
                          num_create_from_string("1.4")};
    matrix_t *A = NULL;
    number_t eigenvalues[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *V = NULL;

    for (size_t i = 0; i < 4; ++i)
        num_set_prec_bits(&A_vals[i], 512u);

    A = mat_create_num(2, 2, A_vals);
    check_bool("high-precision general A not NULL", A != NULL);
    check_bool("high-precision general A type is number", A && mat_typeof(A) == MAT_TYPE_NUMBER);
    if (A) {
        number_t a00 = mat_get_num(A, 0, 0);
        check_bool("high-precision general fixture preserves precision bits", num_get_prec_bits(a00) >= 512u);
        num_destroy(&a00);
    }

    print_mnum("A (high-precision general)", A);

    check_bool("high-precision general eigendecompose rc = 0", A && mat_eigendecompose(A, eigenvalues, &V) == 0);
    check_bool("high-precision general eigenvectors not NULL", V != NULL);
    check_bool("high-precision general eigenvectors type is number", V && mat_typeof(V) == MAT_TYPE_NUMBER);

    if (A && V) {
        matrix_t *Aq = test_mat_evaluate_complex(A);
        matrix_t *Vq = test_mat_evaluate_complex(V);

        print_mnum("V (high-precision general)", V);
        for (int j = 0; Aq && Vq && j < 2; ++j) {
            qcomplex_t lam = num_to_qcomplex(eigenvalues[j]);
            for (size_t i = 0; i < 2; ++i) {
                qcomplex_t Av_ij = QC_ZERO;
                qcomplex_t vij;
                char label[96];

                for (size_t k = 0; k < 2; ++k) {
                    qcomplex_t aik, vkj;
                    mat_get(Aq, i, k, &aik);
                    mat_get(Vq, k, j, &vkj);
                    Av_ij = qc_add(Av_ij, qc_mul(aik, vkj));
                }
                mat_get(Vq, i, j, &vij);

                snprintf(label, sizeof(label), "high-precision general: (Av)[%zu,%d] = lam*v[%zu,%d]", i, j, i, j);
                check_qc_val(label, Av_ij, qc_mul(lam, vij), 1e-27);
            }
        }

        {
            number_t eval0_re = num_real_part(eigenvalues[0]);
            number_t eval1_re = num_real_part(eigenvalues[1]);
            int e0_smaller = num_lt(eval0_re, eval1_re);
            number_t ev_min = e0_smaller ? num_clone(eval0_re) : num_clone(eval1_re);
            number_t ev_max = e0_smaller ? num_clone(eval1_re) : num_clone(eval0_re);
            check_qc_val("high-precision general eigenvalue min = 2+0i", num_to_qcomplex(ev_min),
                         qc_make(qf_from_double(2.0), QF_ZERO), 1e-27);
            check_qc_val("high-precision general eigenvalue max = 5+0i", num_to_qcomplex(ev_max),
                         qc_make(qf_from_double(5.0), QF_ZERO), 1e-27);
            num_destroy(&ev_min);
            num_destroy(&ev_max);
            num_destroy(&eval1_re);
            num_destroy(&eval0_re);
        }

        mat_free(Aq);
        mat_free(Vq);
    }

    {
        number_t B_vals[9] = {
            num_create_from_string("4.0"), num_create_from_string("-1.1"), num_create_from_string("0.8"),
            num_create_from_string("0.0"), num_create_from_string("3.0"),  num_create_from_string("-0.6"),
            num_create_from_string("0.0"), num_create_from_string("0.0"),  num_create_from_string("2.0")};
        matrix_t *B = NULL;
        number_t evals3[3] = {NUM_ZERO, NUM_ZERO, NUM_ZERO};
        matrix_t *W = NULL;

        for (size_t i = 0; i < 9; ++i)
            num_set_prec_bits(&B_vals[i], 512u);

        B = mat_create_num(3, 3, B_vals);
        check_bool("high-precision general 3x3 B not NULL", B != NULL);
        check_bool("high-precision general 3x3 B type is number", B && mat_typeof(B) == MAT_TYPE_NUMBER);
        check_bool("high-precision general 3x3 eigendecompose rc = 0", B && mat_eigendecompose(B, evals3, &W) == 0);
        check_bool("high-precision general 3x3 eigenvectors not NULL", W != NULL);
        check_bool("high-precision general 3x3 eigenvectors type is number", W && mat_typeof(W) == MAT_TYPE_NUMBER);

        if (B && W) {
            matrix_t *Bq = test_mat_evaluate_complex(B);
            matrix_t *Wq = test_mat_evaluate_complex(W);
            check_bool("high-precision general 3x3 evaluated B not NULL", Bq != NULL);
            check_bool("high-precision general 3x3 evaluated W not NULL", Wq != NULL);

            for (int j = 0; Bq && Wq && j < 3; ++j) {
                qcomplex_t lam = num_to_qcomplex(evals3[j]);
                for (size_t i = 0; i < 3; ++i) {
                    qcomplex_t Bw_ij = QC_ZERO;
                    qcomplex_t wij;
                    char label[112];

                    for (size_t k = 0; k < 3; ++k) {
                        qcomplex_t bik, wkj;
                        mat_get(Bq, i, k, &bik);
                        mat_get(Wq, k, j, &wkj);
                        Bw_ij = qc_add(Bw_ij, qc_mul(bik, wkj));
                    }
                    mat_get(Wq, i, j, &wij);

                    snprintf(label, sizeof(label), "high-precision general 3x3: (Bw)[%zu,%d] = lam*w[%zu,%d]", i, j, i,
                             j);
                    check_qc_val(label, Bw_ij, qc_mul(lam, wij), 1e-23);
                }
            }

            mat_free(Bq);
            mat_free(Wq);
        }

        for (size_t i = 0; i < 9; ++i)
            num_destroy(&B_vals[i]);
        num_destroy(&evals3[0]);
        num_destroy(&evals3[1]);
        num_destroy(&evals3[2]);
        mat_free(B);
        mat_free(W);
    }

    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);
    num_destroy(&eigenvalues[0]);
    num_destroy(&eigenvalues[1]);
    mat_free(A);
    mat_free(V);
}

/* ------------------------------------------------------------------ README example */

static void test_mat_simplify_symbolic_helper(void)
{
    expr_t *delta = test_expr_new_named_var_d(1.5, "Δ");
    expr_t *omega = test_expr_new_named_var_d(0.25, "Ω");
    expr_t *prod1 = NULL;
    expr_t *prod2 = NULL;
    expr_t *neg_prod1 = NULL;
    expr_t *entry = NULL;
    expr_t *vals[4] = {NULL, NULL, NULL, NULL};
    matrix_t *A = NULL;
    matrix_t *S = NULL;
    char *text = NULL;

    check_bool("mat_simplify_symbolic helper Δ allocated", delta != NULL);
    check_bool("mat_simplify_symbolic helper Ω allocated", omega != NULL);
    if (!delta || !omega)
        goto cleanup;

    prod1 = expr_mul(delta, omega);

    prod2 = expr_mul(delta, omega);

    neg_prod1 = expr_neg(prod1);
    entry = expr_add(neg_prod1, prod2);

    vals[0] = entry;
    vals[1] = entry;
    vals[2] = entry;
    vals[3] = entry;
    A = mat_create_expr(2, 2, vals);
    check_bool("mat_simplify_symbolic helper source matrix non-null", A != NULL);

    S = mat_simplify_symbolic(A);
    check_bool("mat_simplify_symbolic helper simplified matrix non-null", S != NULL);
    text = S ? mat_to_string(S, MAT_STRING_INLINE_PRETTY) : NULL;
    check_bool("mat_simplify_symbolic helper collapses symbolic zero", text && strcmp(text, "{ (0, 0; 0, 0) }") == 0);

cleanup:
    free(text);
    mat_free(S);
    mat_free(A);
    expr_free(entry);
    expr_free(neg_prod1);
    expr_free(prod2);
    expr_free(prod1);
    expr_free(omega);
    expr_free(delta);
}

/* ------------------------------------------------------------------ generic matrix check (double) */

typedef struct {
    double tol;
    int complex_mode;
} matrix_validity_ctx_t;

static int matrix_validity_equal(const void *actual, const void *expected, void *ctx)
{
    const matrix_t *got = actual;
    const matrix_t *want = expected;
    const matrix_validity_ctx_t *cfg = ctx;
    double tol = cfg ? cfg->tol : 1e-12;
    size_t rows, cols;

    if (!got || !want)
        return 0;

    rows = mat_get_row_count(got);
    cols = mat_get_col_count(got);
    if (rows != mat_get_row_count(want) || cols != mat_get_col_count(want))
        return 0;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            number_t gn = mat_get_num(got, i, j);
            number_t en = mat_get_num(want, i, j);
            number_t diff = num_sub(gn, en);
            number_t abs_diff = num_abs(diff);
            double err = num_to_double(abs_diff);

            num_destroy(&abs_diff);
            num_destroy(&diff);
            num_destroy(&gn);
            num_destroy(&en);

            if (!isfinite(err) || err >= tol)
                return 0;
        }
    }

    return 1;
}

static int matrix_validity_format(const void *value, string_t *out, void *ctx)
{
    const matrix_t *matrix = value;
    char *text;
    (void)ctx;

    if (!out)
        return -1;
    if (!matrix)
        return string_append_cstr(out, "<null>");

    text = mat_to_string(matrix, MAT_STRING_LAYOUT_PRETTY);
    if (!text)
        return string_append_cstr(out, "<format-error>");

    if (string_append_cstr(out, text) != 0) {
        free(text);
        return -1;
    }
    free(text);
    return 0;
}

static matrix_validity_ctx_t matrix_double_default_ctx = {1e-12, 0};
static matrix_validity_ctx_t matrix_mp_real_default_ctx = {1e-12, 0};
static matrix_validity_ctx_t matrix_complex_default_ctx = {1e-12, 1};
static const test_validity_contract_t matrix_double_default_contract = TEST_VALIDITY_CONTRACT(
    "matrix-double-default", matrix_validity_equal, matrix_validity_format, &matrix_double_default_ctx);
static const test_validity_contract_t matrix_mp_real_default_contract = TEST_VALIDITY_CONTRACT(
    "matrix-mp-real-default", matrix_validity_equal, matrix_validity_format, &matrix_mp_real_default_ctx);
static const test_validity_contract_t matrix_complex_default_contract = TEST_VALIDITY_CONTRACT(
    "matrix-complex-default", matrix_validity_equal, matrix_validity_format, &matrix_complex_default_ctx);

const test_validity_contract_t *matrix_validity_contract_double_default(void)
{
    return &matrix_double_default_contract;
}

const test_validity_contract_t *matrix_validity_contract_mp_real_default(void)
{
    return &matrix_mp_real_default_contract;
}

const test_validity_contract_t *matrix_validity_contract_complex_default(void)
{
    return &matrix_complex_default_contract;
}

bool test_assert_matrix_d_close(matrix_t *got, matrix_t *expected, double tol, const char *file, int line)
{
    matrix_validity_ctx_t ctx = {tol, 0};
    test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("matrix-double-close", matrix_validity_equal, matrix_validity_format, &ctx);
    return test_assert_validity(&contract, got, expected, file, line);
}

bool test_assert_matrix_mp_real_close(matrix_t *got, matrix_t *expected, double tol, const char *file, int line)
{
    matrix_validity_ctx_t ctx = {tol, 0};
    test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("matrix-mp-real-close", matrix_validity_equal, matrix_validity_format, &ctx);
    return test_assert_validity(&contract, got, expected, file, line);
}

bool test_assert_matrix_complex_close(matrix_t *got, matrix_t *expected, double tol, const char *file, int line)
{
    matrix_validity_ctx_t ctx = {tol, 1};
    test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("matrix-complex-close", matrix_validity_equal, matrix_validity_format, &ctx);
    return test_assert_validity(&contract, got, expected, file, line);
}

bool test_assert_matrix_d_identity(matrix_t *got, size_t n, double tol, const char *file, int line)
{
    matrix_t *identity = test_mat_identity_d(n);
    bool ok;

    if (!identity) {
        test_mark_failure(file, line, "matrix identity fixture allocation failed");
        return false;
    }

    ok = test_assert_matrix_d_close(got, identity, tol, file, line);
    mat_free(identity);
    return ok;
}

bool test_assert_matrix_mp_real_identity(matrix_t *got, size_t n, double tol, const char *file, int line)
{
    matrix_t *identity = test_mat_identity_d(n);
    bool ok;

    if (!identity) {
        test_mark_failure(file, line, "matrix identity fixture allocation failed");
        return false;
    }

    ok = test_assert_matrix_mp_real_close(got, identity, tol, file, line);
    mat_free(identity);
    return ok;
}

bool test_assert_matrix_complex_identity(matrix_t *got, size_t n, double tol, const char *file, int line)
{
    matrix_t *identity = test_mat_identity_d(n);
    bool ok;

    if (!identity) {
        test_mark_failure(file, line, "matrix identity fixture allocation failed");
        return false;
    }

    ok = test_assert_matrix_complex_close(got, identity, tol, file, line);
    mat_free(identity);
    return ok;
}

/* ------------------------------------------------------------------ matrix comparison helper */

static void check_mat2x2_d(const char *label, matrix_t *R, double e00, double e01, double e10, double e11, double tol)
{
    double ev[4] = {e00, e01, e10, e11};
    matrix_t *E = test_mat_create_d(2, 2, ev);
    if (!E) {
        test_mark_failure(__FILE__, __LINE__, label ? label : "check_mat2x2_d expected allocation failed");
        return;
    }
    (void)test_assert_matrix_d_close(R, E, tol, __FILE__, __LINE__);
    mat_free(E);
}

/* ------------------------------------------------------------------ matrix comparison helpers (qfloat) */

/* ------------------------------------------------------------------ matrix comparison helpers (qcomplex) */

/* ------------------------------------------------------------------ identity helpers (qfloat / qcomplex) */

static void check_unary_jordan_2x2_d(const char *label, matrix_t *(*fun)(const matrix_t *), double a, double fa,
                                     double fpa, double tol);
static void check_unary_diagonal_2x2_complex(const char *label, matrix_t *(*fun)(const matrix_t *), qcomplex_t a,
                                             qcomplex_t b, qcomplex_t fa, qcomplex_t fb, double tol);

static void check_unary_jordan_2x2_mp_real(const char *label, matrix_t *(*fun)(const matrix_t *), qfloat_t a,
                                           qfloat_t fa, qfloat_t fpa, double tol)
{
    number_t avals[4] = {num_create_from_qfloat(a), NUM_ONE, NUM_ZERO, num_create_from_qfloat(a)};
    number_t evals[4] = {num_create_from_qfloat(fa), num_create_from_qfloat(fpa), NUM_ZERO, num_create_from_qfloat(fa)};
    matrix_t *A = mat_create_num(2, 2, avals);
    matrix_t *E = mat_create_num(2, 2, evals);
    num_destroy(&avals[0]);
    num_destroy(&avals[3]);
    num_destroy(&evals[0]);
    num_destroy(&evals[1]);
    num_destroy(&evals[3]);

    check_bool("2x2 qfloat Jordan input allocated", A != NULL);
    check_bool("2x2 qfloat Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mnum("A (qfloat Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_mp_real_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_d(const char *label, matrix_t *(*fun)(const matrix_t *), double a, double b,
                                       double fa, double fb, double tol)
{
    double avals[4] = {a, 0.0, 0.0, b};
    double evals[4] = {fa, 0.0, 0.0, fb};
    matrix_t *A = test_mat_create_d(2, 2, avals);
    matrix_t *E = test_mat_create_d(2, 2, evals);

    check_bool("2x2 diagonal input allocated", A != NULL);
    check_bool("2x2 diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (2x2 diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_d_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_mp_real(const char *label, matrix_t *(*fun)(const matrix_t *), qfloat_t a,
                                             qfloat_t b, qfloat_t fa, qfloat_t fb, double tol)
{
    number_t avals[4] = {num_create_from_qfloat(a), NUM_ZERO, NUM_ZERO, num_create_from_qfloat(b)};
    number_t evals[4] = {num_create_from_qfloat(fa), NUM_ZERO, NUM_ZERO, num_create_from_qfloat(fb)};
    matrix_t *A = mat_create_num(2, 2, avals);
    matrix_t *E = mat_create_num(2, 2, evals);
    num_destroy(&avals[0]);
    num_destroy(&avals[3]);
    num_destroy(&evals[0]);
    num_destroy(&evals[3]);

    check_bool("2x2 qfloat diagonal input allocated", A != NULL);
    check_bool("2x2 qfloat diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mnum("A (2x2 qfloat diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_mp_real_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void test_mat_special_unary_extensions(void)
{
    printf(C_CYAN "TEST: extended unary matrix special functions\n" C_RESET);

    {
        qfloat_t x = qf_from_double(0.25);
        qfloat_t y = qf_erfinv(x);
        qfloat_t fp = qf_mul(qf_from_double(0.5), qf_mul(QF_SQRT_PI, qf_exp(qf_mul(y, y))));
        check_unary_jordan_2x2_d("erfinv(aI+N)=erfinv(a)I+erfinv'(a)N", mat_erfinv, 0.25, qf_to_double(y),
                                 qf_to_double(fp), 1e-12);
    }

    {
        qfloat_t x = qf_from_double(0.75);
        qfloat_t y = qf_erfcinv(x);
        qfloat_t fp = qf_neg(qf_mul(qf_from_double(0.5), qf_mul(QF_SQRT_PI, qf_exp(qf_mul(y, y)))));
        check_unary_jordan_2x2_d("erfcinv(aI+N)=erfcinv(a)I+erfcinv'(a)N", mat_erfcinv, 0.75, qf_to_double(y),
                                 qf_to_double(fp), 1e-12);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        check_unary_jordan_2x2_d("lgamma(aI+N)=lgamma(a)I+digamma(a)N", mat_lgamma, 2.5, qf_to_double(qf_lgamma(x)),
                                 qf_to_double(qf_digamma(x)), 1e-12);

        check_unary_diagonal_2x2_d("tetragamma(diag(a,b)) = diag(tetragamma(a), tetragamma(b))", mat_tetragamma, 2.5,
                                   1.75, qf_to_double(qf_tetragamma(qf_from_double(2.5))),
                                   qf_to_double(qf_tetragamma(qf_from_double(1.75))), 1e-12);
    }

    {
        qfloat_t a = qf_from_double(1.329340388179137);
        qfloat_t y = qf_gammainv(a);
        qfloat_t fp = qf_div(QF_ONE, qf_mul(a, qf_digamma(y)));
        check_unary_jordan_2x2_d("gammainv(aI+N)=gammainv(a)I+gammainv'(a)N", mat_gammainv, 1.329340388179137,
                                 qf_to_double(y), qf_to_double(fp), 1e-10);
    }

    {
        qfloat_t x = qf_from_double(0.5);
        qfloat_t pdf = qf_normal_pdf(x);
        check_unary_jordan_2x2_d("normal_pdf(aI+N)=pdf(a)I+pdf'(a)N", mat_normal_pdf, 0.5, qf_to_double(pdf),
                                 qf_to_double(qf_neg(qf_mul(x, pdf))), 1e-12);

        check_unary_jordan_2x2_d("normal_cdf(aI+N)=cdf(a)I+cdf'(a)N", mat_normal_cdf, 0.5,
                                 qf_to_double(qf_normal_cdf(x)), qf_to_double(pdf), 1e-12);

        check_unary_jordan_2x2_d("normal_logpdf(aI+N)=logpdf(a)I+logpdf'(a)N", mat_normal_logpdf, 0.5,
                                 qf_to_double(qf_normal_logpdf(x)), qf_to_double(qf_neg(x)), 1e-12);
    }

    {
        qfloat_t a = qf_from_double(0.2);
        qfloat_t b = qf_from_double(-0.05);
        check_unary_diagonal_2x2_d("productlog(diag(a,b)) = diag(W0(a),W0(b))", mat_productlog, 0.2, -0.05,
                                   qf_to_double(qf_productlog(a)), qf_to_double(qf_productlog(b)), 1e-12);

        check_unary_diagonal_2x2_mp_real("mp-real productlog(diag(a,b)) = diag(W0(a),W0(b))", mat_productlog, a, b,
                                         qf_productlog(a), qf_productlog(b), 1e-25);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t a = qf_from_double(0.2);
        qfloat_t b = qf_from_double(-0.2);
        qfloat_t ei_x = qf_from_double(0.5);

        check_unary_jordan_2x2_mp_real("mp-real digamma(aI+N)=digamma(a)I+trigamma(a)N", mat_digamma, x, qf_digamma(x),
                                       qf_trigamma(x), 1e-25);

        check_unary_jordan_2x2_mp_real("mp-real lambert_w0(aI+N)=W0(a)I+W0'(a)N", mat_lambert_w0, a, qf_lambert_w0(a),
                                       qf_div(qf_lambert_w0(a), qf_mul(a, qf_add(QF_ONE, qf_lambert_w0(a)))), 1e-25);

        check_unary_jordan_2x2_mp_real("mp-real lambert_wm1(aI+N)=Wm1(a)I+Wm1'(a)N", mat_lambert_wm1, b,
                                       qf_lambert_wm1(b),
                                       qf_div(qf_lambert_wm1(b), qf_mul(b, qf_add(QF_ONE, qf_lambert_wm1(b)))), 1e-25);

        check_unary_jordan_2x2_mp_real("mp-real ei(aI+N)=Ei(a)I+Ei'(a)N", mat_Ei, ei_x, qf_Ei(ei_x),
                                       qf_div(qf_exp(ei_x), ei_x), 1e-25);
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(-0.05), qf_from_double(0.08));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), QF_ZERO);
        qcomplex_t wm1b = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));

        check_unary_diagonal_2x2_complex("complex gamma(diag(z1,z2)) = diag(gamma(z1),gamma(z2))", mat_gamma, z1, z2,
                                         qc_gamma(z1), qc_gamma(z2), 1e-24);

        check_unary_diagonal_2x2_complex("complex digamma(diag(z1,z2)) = diag(digamma(z1),digamma(z2))", mat_digamma,
                                         z1, z2, qc_digamma(z1), qc_digamma(z2), 1e-24);

        check_unary_diagonal_2x2_complex("complex productlog(diag(a,b)) = diag(W0(a),W0(b))", mat_productlog, w0a, w0b,
                                         qc_productlog(w0a), qc_productlog(w0b), 1e-24);

        check_unary_diagonal_2x2_complex("complex lambert_wm1(diag(a,b)) = diag(Wm1(a),Wm1(b))", mat_lambert_wm1, wm1a,
                                         wm1b, qc_make(qf_lambert_wm1(qf_from_double(-0.2)), QF_ZERO),
                                         qc_lambert_wm1(wm1b), 1e-24);

        check_unary_diagonal_2x2_complex("complex ei(diag(z1,z2)) = diag(Ei(z1),Ei(z2))", mat_Ei, z1, z2, qc_Ei(z1),
                                         qc_Ei(z2), 1e-24);
        check_unary_diagonal_2x2_complex("complex Li(diag(z1,z2)) = diag(Li(z1),Li(z2))", mat_Li, z1, z2, qc_Li(z1),
                                         qc_Li(z2), 1e-24);
    }
}

static void check_unary_jordan_2x2_d(const char *label, matrix_t *(*fun)(const matrix_t *), double a, double fa,
                                     double fpa, double tol)
{
    double avals[4] = {a, 1.0, 0.0, a};
    double evals[4] = {fa, fpa, 0.0, fa};
    matrix_t *A = test_mat_create_d(2, 2, avals);
    matrix_t *E = test_mat_create_d(2, 2, evals);

    check_bool("2x2 Jordan input allocated", A != NULL);
    check_bool("2x2 Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_d_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_complex(const char *label, matrix_t *(*fun)(const matrix_t *), qcomplex_t a,
                                             qcomplex_t b, qcomplex_t fa, qcomplex_t fb, double tol)
{
    qcomplex_t avals[4] = {a, QC_ZERO, QC_ZERO, b};
    qcomplex_t evals[4] = {fa, QC_ZERO, QC_ZERO, fb};
    matrix_t *A = test_mat_create_complex(2, 2, avals);
    matrix_t *E = test_mat_create_complex(2, 2, evals);

    check_bool("2x2 qcomplex diagonal input allocated", A != NULL);
    check_bool("2x2 qcomplex diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqc("A (qcomplex diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_complex_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_jordan_3x3_d(const char *label, matrix_t *(*fun)(const matrix_t *), double a, double fa,
                                     double fpa, double fppa_over_2, double tol)
{
    double avals[9] = {a, 1.0, 0.0, 0.0, a, 1.0, 0.0, 0.0, a};
    double evals[9] = {fa, fpa, fppa_over_2, 0.0, fa, fpa, 0.0, 0.0, fa};
    matrix_t *A = test_mat_create_d(3, 3, avals);
    matrix_t *E = test_mat_create_d(3, 3, evals);

    check_bool("3x3 Jordan input allocated", A != NULL);
    check_bool("3x3 Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (3x3 Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_d_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_3x3_complex(const char *label, matrix_t *(*fun)(const matrix_t *), qcomplex_t a,
                                             qcomplex_t b, qcomplex_t c, qcomplex_t fa, qcomplex_t fb, qcomplex_t fc,
                                             double tol)
{
    qcomplex_t avals[9] = {a, QC_ZERO, QC_ZERO, QC_ZERO, b, QC_ZERO, QC_ZERO, QC_ZERO, c};
    qcomplex_t evals[9] = {fa, QC_ZERO, QC_ZERO, QC_ZERO, fb, QC_ZERO, QC_ZERO, QC_ZERO, fc};
    matrix_t *A = test_mat_create_complex(3, 3, avals);
    matrix_t *E = test_mat_create_complex(3, 3, evals);

    check_bool("3x3 qcomplex diagonal input allocated", A != NULL);
    check_bool("3x3 qcomplex diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqc("A (3x3 qcomplex diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        (void)test_assert_matrix_complex_close(R, E, tol, __FILE__, __LINE__);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void test_mat_special_unary_square_extensions(void)
{
    printf(C_CYAN "TEST: extended unary functions on square matrices\n" C_RESET);

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t gamma_x = qf_gamma(x);
        qfloat_t digamma_x = qf_digamma(x);
        qfloat_t trigamma_x = qf_trigamma(x);
        qfloat_t one = qf_from_double(1.0);

        check_unary_jordan_2x2_d("gamma(aI+N)=gamma(a)I+gamma'(a)N", mat_gamma, 2.5, qf_to_double(gamma_x),
                                 qf_to_double(qf_mul(gamma_x, digamma_x)), 1e-12);

        check_unary_jordan_2x2_d("digamma(aI+N)=digamma(a)I+trigamma(a)N", mat_digamma, 2.5, qf_to_double(digamma_x),
                                 qf_to_double(trigamma_x), 1e-12);

        check_unary_jordan_2x2_d("trigamma(aI+N)=trigamma(a)I+tetragamma(a)N", mat_trigamma, 2.5,
                                 qf_to_double(trigamma_x), qf_to_double(qf_tetragamma(x)), 1e-12);

        {
            qfloat_t a = qf_from_double(0.2);
            qfloat_t w = qf_lambert_w0(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            check_unary_jordan_2x2_d("lambert_w0(aI+N)=W0(a)I+W0'(a)N", mat_lambert_w0, 0.2, qf_to_double(w),
                                     qf_to_double(wp), 1e-12);
        }

        {
            qfloat_t a = qf_from_double(-0.2);
            qfloat_t w = qf_lambert_wm1(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            check_unary_jordan_2x2_d("lambert_wm1(aI+N)=Wm1(a)I+Wm1'(a)N", mat_lambert_wm1, -0.2, qf_to_double(w),
                                     qf_to_double(wp), 1e-12);
        }

        {
            qfloat_t a = qf_from_double(0.5);
            check_unary_jordan_2x2_d("ei(aI+N)=Ei(a)I+Ei'(a)N", mat_Ei, 0.5, qf_to_double(qf_Ei(a)),
                                     qf_to_double(qf_div(qf_exp(a), a)), 1e-12);
        }

        {
            qfloat_t a = qf_from_double(1.0);
            check_unary_jordan_2x2_d("e1(aI+N)=E1(a)I+E1'(a)N", mat_E1, 1.0, qf_to_double(qf_E1(a)),
                                     qf_to_double(qf_div(qf_neg(qf_exp(qf_neg(a))), a)), 1e-12);
        }
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(0.1), qf_from_double(0.05));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));
        qcomplex_t wm1b = qc_make(qf_from_double(-0.1), qf_from_double(-0.05));

        check_unary_diagonal_2x2_complex("complex gamma(diag(z1,z2)) = diag(gamma(z1),gamma(z2))", mat_gamma, z1, z2,
                                         qc_gamma(z1), qc_gamma(z2), 1e-24);

        check_unary_diagonal_2x2_complex("complex digamma(diag(z1,z2)) = diag(digamma(z1),digamma(z2))", mat_digamma,
                                         z1, z2, qc_digamma(z1), qc_digamma(z2), 1e-24);

        check_unary_diagonal_2x2_complex("complex lambert_w0(diag(a,b)) = diag(W0(a),W0(b))", mat_lambert_w0, w0a, w0b,
                                         qc_productlog(w0a), qc_productlog(w0b), 1e-24);

        check_unary_diagonal_2x2_complex("complex lambert_wm1(diag(a,b)) = diag(Wm1(a),Wm1(b))", mat_lambert_wm1, wm1a,
                                         wm1b, qc_lambert_wm1(wm1a), qc_lambert_wm1(wm1b), 1e-24);

        check_unary_diagonal_2x2_complex("complex ei(diag(z1,z2)) = diag(Ei(z1),Ei(z2))", mat_Ei, z1, z2, qc_Ei(z1),
                                         qc_Ei(z2), 1e-24);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t gamma_x = qf_gamma(x);
        qfloat_t digamma_x = qf_digamma(x);
        qfloat_t trigamma_x = qf_trigamma(x);
        qfloat_t tetragamma_x = qf_tetragamma(x);
        qfloat_t one = qf_from_double(1.0);

        check_unary_jordan_3x3_d("gamma(aI+N+N^2)=gamma(a)I+gamma'(a)N+gamma''(a)N^2/2", mat_gamma, 2.5,
                                 qf_to_double(gamma_x), qf_to_double(qf_mul(gamma_x, digamma_x)),
                                 qf_to_double(qf_mul(gamma_x, qf_add(trigamma_x, qf_mul(digamma_x, digamma_x)))) / 2.0,
                                 1e-12);

        check_unary_jordan_3x3_d("digamma(aI+N+N^2)=digamma(a)I+trigamma(a)N+tetragamma(a)N^2/2", mat_digamma, 2.5,
                                 qf_to_double(digamma_x), qf_to_double(trigamma_x), qf_to_double(tetragamma_x) / 2.0,
                                 1e-12);

        {
            qfloat_t a = qf_from_double(0.2);
            qfloat_t w = qf_lambert_w0(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            qfloat_t wpp =
                qf_neg(qf_div(qf_mul(qf_mul(w, w), qf_add(w, qf_from_double(2.0))),
                              qf_mul(qf_mul(a, a), qf_mul(qf_add(one, w), qf_mul(qf_add(one, w), qf_add(one, w))))));
            check_unary_jordan_3x3_d("lambert_w0(aI+N+N^2)=W0(a)I+W0'(a)N+W0''(a)N^2/2", mat_lambert_w0, 0.2,
                                     qf_to_double(w), qf_to_double(wp), qf_to_double(wpp) / 2.0, 1e-12);
        }

        {
            qfloat_t a = qf_from_double(-0.2);
            qfloat_t w = qf_lambert_wm1(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            qfloat_t wpp =
                qf_neg(qf_div(qf_mul(qf_mul(w, w), qf_add(w, qf_from_double(2.0))),
                              qf_mul(qf_mul(a, a), qf_mul(qf_add(one, w), qf_mul(qf_add(one, w), qf_add(one, w))))));
            check_unary_jordan_3x3_d("lambert_wm1(aI+N+N^2)=Wm1(a)I+Wm1'(a)N+Wm1''(a)N^2/2", mat_lambert_wm1, -0.2,
                                     qf_to_double(w), qf_to_double(wp), qf_to_double(wpp) / 2.0, 1e-12);
        }

        {
            qfloat_t a = qf_from_double(0.5);
            check_unary_jordan_3x3_d("ei(aI+N+N^2)=Ei(a)I+Ei'(a)N+Ei''(a)N^2/2", mat_Ei, 0.5, qf_to_double(qf_Ei(a)),
                                     qf_to_double(qf_div(qf_exp(a), a)),
                                     qf_to_double(qf_div(qf_mul(qf_exp(a), qf_sub(a, one)), qf_mul(a, a))) / 2.0,
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(1.0);
            check_unary_jordan_3x3_d(
                "e1(aI+N+N^2)=E1(a)I+E1'(a)N+E1''(a)N^2/2", mat_E1, 1.0, qf_to_double(qf_E1(a)),
                qf_to_double(qf_div(qf_neg(qf_exp(qf_neg(a))), a)),
                qf_to_double(qf_div(qf_mul(qf_exp(qf_neg(a)), qf_add(a, one)), qf_mul(a, a))) / 2.0, 1e-12);
        }
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t z3 = qc_make(qf_from_double(0.8), qf_from_double(-0.25));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(0.1), qf_from_double(0.05));
        qcomplex_t w0c = qc_make(qf_from_double(0.15), qf_from_double(-0.08));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));
        qcomplex_t wm1b = qc_make(qf_from_double(-0.1), qf_from_double(-0.05));
        qcomplex_t wm1c = qc_make(qf_from_double(-0.16), qf_from_double(-0.07));

        check_unary_diagonal_3x3_complex("complex gamma(diag(z1,z2,z3)) = diag(gamma(z1),gamma(z2),gamma(z3))",
                                         mat_gamma, z1, z2, z3, qc_gamma(z1), qc_gamma(z2), qc_gamma(z3), 1e-24);

        check_unary_diagonal_3x3_complex("complex digamma(diag(z1,z2,z3)) = diag(digamma(z1),digamma(z2),digamma(z3))",
                                         mat_digamma, z1, z2, z3, qc_digamma(z1), qc_digamma(z2), qc_digamma(z3),
                                         1e-24);

        check_unary_diagonal_3x3_complex("complex lambert_w0(diag(a,b,c)) = diag(W0(a),W0(b),W0(c))", mat_lambert_w0,
                                         w0a, w0b, w0c, qc_productlog(w0a), qc_productlog(w0b), qc_productlog(w0c),
                                         1e-24);

        check_unary_diagonal_3x3_complex("complex lambert_wm1(diag(a,b,c)) = diag(Wm1(a),Wm1(b),Wm1(c))",
                                         mat_lambert_wm1, wm1a, wm1b, wm1c, qc_lambert_wm1(wm1a), qc_lambert_wm1(wm1b),
                                         qc_lambert_wm1(wm1c), 1e-24);

        check_unary_diagonal_3x3_complex("complex ei(diag(z1,z2,z3)) = diag(Ei(z1),Ei(z2),Ei(z3))", mat_Ei, z1, z2, z3,
                                         qc_Ei(z1), qc_Ei(z2), qc_Ei(z3), 1e-24);
    }
}

static void test_mat_neg_convenience(void)
{
    printf(C_CYAN "TEST: mat_neg convenience wrapper\n" C_RESET);

    double exprs[4] = {1.0, -2.0, 3.5, 0.0};
    double dexp[4] = {-1.0, 2.0, -3.5, 0.0};
    matrix_t *A = test_mat_create_d(2, 2, exprs);
    matrix_t *E = test_mat_create_d(2, 2, dexp);
    check_bool("double neg input allocated", A != NULL);
    check_bool("double neg expected allocated", E != NULL);
    if (A && E) {
        print_md("A", A);
        matrix_t *N = mat_neg(A);
        check_bool("mat_neg(double) not NULL", N != NULL);
        if (N) {
            if (!test_assert_matrix_d_close(N, E, 1e-30, __FILE__, __LINE__)) {
                mat_free(N);
                mat_free(A);
                mat_free(E);
                return;
            }
            mat_free(N);
        }
    }
    mat_free(A);
    mat_free(E);

    qcomplex_t qvals[2] = {qc_make(qf_from_double(1.0), qf_from_double(-2.0)),
                           qc_make(qf_from_double(-0.5), qf_from_double(0.25))};
    qcomplex_t qexp[2] = {qc_make(qf_from_double(-1.0), qf_from_double(2.0)),
                          qc_make(qf_from_double(0.5), qf_from_double(-0.25))};
    matrix_t *Q = test_mat_create_complex(1, 2, qvals);
    matrix_t *QE = test_mat_create_complex(1, 2, qexp);
    check_bool("qcomplex neg input allocated", Q != NULL);
    check_bool("qcomplex neg expected allocated", QE != NULL);
    if (Q && QE) {
        print_mqc("Q", Q);
        matrix_t *QN = mat_neg(Q);
        check_bool("mat_neg(qcomplex) not NULL", QN != NULL);
        if (QN) {
            if (!test_assert_matrix_complex_close(QN, QE, 1e-28, __FILE__, __LINE__)) {
                mat_free(QN);
                mat_free(Q);
                mat_free(QE);
                return;
            }
            mat_free(QN);
        }
    }
    mat_free(Q);
    mat_free(QE);

    {
        matrix_t *S = test_mat_sparse_d(2, 2);
        matrix_t *SE = NULL;
        double vals[4] = {0.0, 3.0, -2.0, 0.0};
        double minus_three = -3.0, two = 2.0;

        check_bool("sparse neg input allocated", S != NULL);
        if (S) {
            mat_set(S, 0, 1, &minus_three);
            mat_set(S, 1, 0, &two);
            SE = test_mat_create_d(2, 2, vals);
            print_md("S", S);
            matrix_t *SN = mat_neg(S);
            check_bool("mat_neg(sparse) not NULL", SN != NULL);
            check_bool("mat_neg(sparse) stays sparse", SN && mat_is_sparse(SN));
            if (SN) {
                if (!test_assert_matrix_d_close(SN, SE, 1e-30, __FILE__, __LINE__)) {
                    mat_free(SN);
                    mat_free(SE);
                    mat_free(S);
                    return;
                }
                mat_free(SN);
            }
        }

        mat_free(SE);
        mat_free(S);
    }
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
    matrix_t *N = test_mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;
    print_md("N", N);

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
        matrix_t *IN = test_mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_sqrt(IN);
        print_md("sqrt(I+N)", R);
        check_mat2x2_d("sqrt(I+N)", R, 1.0, 0.5, 0.0, 1.0, 1e-12);
        mat_free(IN);
        mat_free(R);
    }

    /* log(I+N) = N = [[0,1],[0,0]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = test_mat_create_d(2, 2, invals);
        print_md("I+N", IN);
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
    matrix_t *A = test_mat_create_d(2, 2, avals);
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
        if (S && C) {
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            check_bool("sin²(A) not NULL", S2 != NULL);
            check_bool("cos²(A) not NULL", C2 != NULL);
            if (S2 && C2) {
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
        if (CH && SH) {
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            check_bool("cosh²(A) not NULL", CH2 != NULL);
            check_bool("sinh²(A) not NULL", SH2 != NULL);
            if (CH2 && SH2) {
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
        matrix_t *negA = test_mat_create_d(2, 2, negvals);
        matrix_t *EnA = mat_exp(negA);
        check_bool("exp(A) not NULL", E != NULL);
        check_bool("exp(-A) not NULL", EnA != NULL);
        if (E && EnA) {
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *L = mat_log(A);
        check_bool("log(A) not NULL", L != NULL);
        if (L) {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) not NULL", R != NULL);
            if (R) {
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("asinh(A) not NULL", S != NULL);
        if (S) {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R) {
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("sqrt(A) not NULL", S != NULL);
        if (S) {
            print_md("sqrt(A)", S);
            matrix_t *R = mat_mul(S, S);
            check_bool("sqrt(A)·sqrt(A) not NULL", R != NULL);
            if (R) {
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("tan(A) not NULL", T != NULL);
        if (T) {
            print_md("tan(A)", T);
            matrix_t *R = mat_atan(T);
            check_bool("atan(tan(A)) not NULL", R != NULL);
            if (R) {
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
    matrix_t *N = test_mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;
    print_md("N", N);

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
    matrix_t *IN = test_mat_create_d(2, 2, invals);
    check_bool("I+N allocated", IN != NULL);
    if (!IN)
        return;
    print_md("I+N", IN);

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
        double exprs[4] = {2.0, 0.0, 0.0, 3.0};
        matrix_t *D = test_mat_create_d(2, 2, exprs);
        print_md("D", D);
        matrix_t *R = mat_pow_int(D, 4);
        print_md("diag(2,3)^4", R);
        check_mat2x2_d("diag(2,3)^4=diag(16,81)", R, 16.0, 0.0, 0.0, 81.0, 1e-12);
        mat_free(D);
        mat_free(R);
    }

    /* symbolic Jordan block: [[x,1],[0,x]]^n */
    {
        mat_bindings_t *bindings = NULL;
        matrix_t *J = mat_from_string_expr("(x, 1; 0, x)", &bindings);
        matrix_t *J2 = NULL;
        matrix_t *J3 = NULL;
        char *j2_text = NULL;
        char *j3_text = NULL;

        check_bool("symbolic Jordan block allocated", J != NULL);
        check_bool("symbolic Jordan block bindings returned", bindings != NULL);
        check_bool("symbolic Jordan block x binding present", bindings && mat_bindings_get(bindings, "x") != NULL);

        J2 = mat_pow_int(J, 2);
        J3 = mat_pow_int(J, 3);
        check_bool("symbolic Jordan block squared", J2 != NULL);
        check_bool("symbolic Jordan block cubed", J3 != NULL);

        j2_text = J2 ? mat_to_string(J2, MAT_STRING_INLINE_PRETTY) : NULL;
        j3_text = J3 ? mat_to_string(J3, MAT_STRING_INLINE_PRETTY) : NULL;

        check_bool("symbolic Jordan block J^2 contains x²", j2_text && strstr(j2_text, "x²") != NULL);
        check_bool("symbolic Jordan block J^2 contains 2x", j2_text && strstr(j2_text, "2x") != NULL);
        check_bool("symbolic Jordan block J^3 contains x³", j3_text && strstr(j3_text, "x³") != NULL);
        check_bool("symbolic Jordan block J^3 contains 3x²", j3_text && strstr(j3_text, "3x²") != NULL);

        free(j3_text);
        free(j2_text);
        mat_free(J3);
        mat_free(J2);
        mat_bindings_free(bindings);
        mat_free(J);
    }
}

/* ------------------------------------------------------------------ mat_pow tests */

static void test_mat_pow_num(void)
{
    printf(C_CYAN "TEST: mat_pow (number_t)\n" C_RESET);

    /* null safety */
    {
        number_t one = num_create_from_double(1.0);
        matrix_t *I = mat_create_identity_num(2);
        check_bool("mat_pow(NULL,&1) = NULL", mat_pow(NULL, &one) == NULL);
        check_bool("mat_pow(A,NULL) = NULL", mat_pow(I, NULL) == NULL);
        mat_free(I);
        num_destroy(&one);
    }

    /* (I+N)^0.5 = I + 0.5·N = [[1,0.5],[0,1]] */
    {
        number_t half = num_create_from_double(0.5);
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = test_mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_pow(IN, &half);
        print_md("(I+N)^0.5", R);
        check_mat2x2_d("(I+N)^0.5", R, 1.0, 0.5, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
        num_destroy(&half);
    }

    /* (I+N)^2.0 = I + 2N = [[1,2],[0,1]] */
    {
        number_t two = num_create_from_double(2.0);
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = test_mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_pow(IN, &two);
        print_md("(I+N)^2.0", R);
        check_mat2x2_d("(I+N)^2.0", R, 1.0, 2.0, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
        num_destroy(&two);
    }

    /* A^1.0 = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        number_t one = num_create_from_double(1.0);
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A (positive-definite)", A);
        matrix_t *R = mat_pow(A, &one);
        print_md("A^1.0", R);
        check_mat2x2_d("A^1.0=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
        mat_free(A);
        mat_free(R);
        num_destroy(&one);
    }

    /* pow(pow_int): (A^2.0)[i,j] ≈ (A²)[i,j] for positive-definite A */
    {
        number_t two = num_create_from_double(2.0);
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A (positive-definite)", A);
        matrix_t *Rp = mat_pow(A, &two);
        matrix_t *Ri = mat_pow_int(A, 2);
        check_bool("A^2.0 not NULL", Rp != NULL);
        check_bool("A^2   not NULL", Ri != NULL);
        if (Rp && Ri) {
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
        num_destroy(&two);
    }
}

/* ------------------------------------------------------------------ mat_erf tests */

static void test_mat_erf_d(void)
{
    printf(C_CYAN "TEST: mat_erf (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_erf(NULL) = NULL", mat_erf(NULL) == NULL);

    /* nilpotent: erf(N) = (2/√π)·N = [[0, 2/√π],[0,0]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = test_mat_create_d(2, 2, nvals);
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        matrix_t *nA = test_mat_create_d(2, 2, navals);
        print_md("A", A);
        print_md("-A", nA);
        matrix_t *E = mat_erf(A);
        matrix_t *En = mat_erf(nA);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erf(-A) not NULL", En != NULL);
        if (E && En) {
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

    /* nilpotent: erfc(N) = I - (2/√π)·N = [[1,-2/√π],[0,1]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = test_mat_create_d(2, 2, nvals);
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
        matrix_t *A = test_mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *E = mat_erf(A);
        matrix_t *EC = mat_erfc(A);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erfc(A) not NULL", EC != NULL);
        if (E && EC) {
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

    matrix_t *An = matsq_new_num(2);
    matrix_t *Adv = test_mat_square_expr(2);

    check_bool("mat_typeof(number)   = MAT_TYPE_NUMBER", An != NULL && mat_typeof(An) == MAT_TYPE_NUMBER);
    check_bool("mat_typeof(expr)     = MAT_TYPE_EXPR", Adv != NULL && mat_typeof(Adv) == MAT_TYPE_EXPR);

    matrix_t *In = mat_create_identity_num(2);

    check_bool("mat_typeof(identity number)   = MAT_TYPE_NUMBER", In != NULL && mat_typeof(In) == MAT_TYPE_NUMBER);

    /* matrix functions preserve element type */
    number_t nvals[4] = {num_create_from_double(0.5), num_create_from_double(0.1), num_create_from_double(0.1),
                         num_create_from_double(0.6)};
    matrix_t *A_n = mat_create_num(2, 2, nvals);
    matrix_t *E_n;

    for (size_t i = 0; i < 4; ++i)
        num_destroy(&nvals[i]);

    E_n = mat_exp(A_n);

    check_bool("mat_exp(number) → number", E_n != NULL && mat_typeof(E_n) == MAT_TYPE_NUMBER);

    mat_free(An);
    mat_free(Adv);
    mat_free(In);
    mat_free(A_n);
    mat_free(E_n);
}

static char *matrix_functions_num_to_cstr(const number_t value)
{
    string_t *text = num_to_string(value);
    string_view_t view;
    size_t len;
    char *out;

    if (!text)
        return NULL;

    view = string_view_all(text);
    len = string_view_length(view);
    out = malloc(len + 1u);
    if (out) {
        memcpy(out, string_c_str(text), len);
        out[len] = '\0';
    }
    string_free(text);
    return out;
}

static char *format_matrix_number_at_own_precision(const number_t value)
{
    char fmt[32];
    char *out;
    size_t significant_digits = num_get_prec_digits(value);
    int needed;
    size_t precision;

    if (num_is_exact(value) || significant_digits == 0u)
        return matrix_functions_num_to_cstr(value);

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

static char *format_matrix_number_for_test_output(const number_t value)
{
    char *text = format_matrix_number_at_own_precision(value);

    if (text)
        return text;
    text = matrix_functions_num_to_cstr(value);
    if (text)
        return text;
    return strdup("(unavailable)");
}

static char *format_matrix_error_for_test_output(const number_t value)
{
    int needed = num_sprintf(NULL, 0u, "%.6N", value);
    char *out;

    if (needed < 0) {
        out = matrix_functions_num_to_cstr(value);
        if (out)
            return out;
        return strdup("(unavailable)");
    }
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, "%.6N", value) < 0) {
        free(out);
        out = matrix_functions_num_to_cstr(value);
        if (out)
            return out;
        return strdup("(unavailable)");
    }
    return out;
}

static number_t matrix_number_error_magnitude(const number_t got, const number_t expected)
{
    number_t promoted_got = num_clone(got);
    number_t diff;
    number_t error;

    if (num_get_prec_bits(expected) > 0u)
        (void)num_set_prec_bits(&promoted_got, num_get_prec_bits(expected));
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
        number_t mag;

        if (num_eq(imag, NUM_ZERO)) {
            mag = num_abs(real);
            num_destroy(&imag);
            num_destroy(&real);
            num_destroy(&diff);
            return mag;
        }

        mag = num_hypot(real, imag);

        num_destroy(&imag);
        num_destroy(&real);
        num_destroy(&diff);
        return mag;
    }
}

static void print_matrix_precision_comparison(const char *label, const number_t got, const number_t expected)
{
    char *expected_text = NULL;
    char *got_text = NULL;
    char *error_text = NULL;
    number_t error;

    expected_text = format_matrix_number_for_test_output(expected);
    got_text = format_matrix_number_for_test_output(got);
    error = matrix_number_error_magnitude(got, expected);
    error_text = format_matrix_error_for_test_output(error);

    ASSERT_NOT_NULL(expected_text);
    ASSERT_NOT_NULL(got_text);
    ASSERT_NOT_NULL(error_text);

    printf("    %s\n", label);
    printf("        expected = %s\n", expected_text);
    printf("        got      = %s\n", got_text);
    printf("        error    = %s\n", error_text);
    printf("        precision: %zu bits, %zu significant digits\n", num_get_prec_bits(got), num_get_prec_digits(got));

    free(error_text);
    num_destroy(&error);
    free(got_text);
    free(expected_text);
}

static void check_number_upper_jordan_from_expr(const char *label, matrix_t *(*mat_fun)(const matrix_t *),
                                                expr_t *(*build_expr)(const expr_t *), const char *lambda_text,
                                                size_t prec_bits, const char *tol_text)
{
    number_t jordan_data[4];
    matrix_t *A = NULL;
    matrix_t *R = NULL;
    expr_t *x = NULL;
    expr_t *expr = NULL;
    expr_t *deriv = NULL;
    number_t lambda;
    number_t expected_diag;
    number_t expected_offdiag;
    number_t got;
    number_t err;
    number_t tol;

    jordan_data[0] = num_create_from_string(lambda_text);
    jordan_data[1] = num_create_from_long(1);
    jordan_data[2] = num_create_from_long(0);
    jordan_data[3] = num_create_from_string(lambda_text);
    num_set_prec_bits(&jordan_data[0], prec_bits);
    num_set_prec_bits(&jordan_data[3], prec_bits);

    A = mat_create_num(2, 2, jordan_data);
    check_bool("number upper Jordan input allocated", A != NULL);
    R = A ? mat_fun(A) : NULL;
    check_bool(label, R != NULL && mat_typeof(R) == MAT_TYPE_NUMBER);

    lambda = num_clone(jordan_data[0]);
    x = expr_new_named_var(lambda, "x");
    expr = x ? build_expr(x) : NULL;
    deriv = expr ? expr_create_deriv(expr, x) : NULL;
    check_bool("number upper Jordan expr helper expr allocated", x != NULL && expr != NULL && deriv != NULL);

    if (R && expr && deriv) {
        tol = num_create_from_string(tol_text);
        expected_diag = expr_eval(expr);
        expected_offdiag = expr_eval(deriv);

        got = mat_get_num(R, 0, 0);
        err = matrix_number_error_magnitude(got, expected_diag);
        check_bool("number upper Jordan [0,0] matches scalar value", num_le(err, tol));
        check_bool("number upper Jordan [0,0] preserves precision", num_get_prec_bits(got) >= prec_bits);
        print_matrix_precision_comparison(label, got, expected_diag);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(R, 0, 1);
        err = matrix_number_error_magnitude(got, expected_offdiag);
        check_bool("number upper Jordan [0,1] matches scalar derivative", num_le(err, tol));
        check_bool("number upper Jordan [0,1] preserves precision", num_get_prec_bits(got) >= prec_bits);
        print_matrix_precision_comparison("    derivative entry", got, expected_offdiag);
        num_destroy(&err);
        num_destroy(&got);

        check_bool("number upper Jordan preserves upper-triangular structure", mat_is_upper_triangular(R));

        num_destroy(&expected_offdiag);
        num_destroy(&expected_diag);
        num_destroy(&tol);
    }

    expr_free(deriv);
    expr_free(expr);
    expr_free(x);
    num_destroy(&lambda);
    mat_free(R);
    mat_free(A);
    num_destroy(&jordan_data[0]);
    num_destroy(&jordan_data[1]);
    num_destroy(&jordan_data[2]);
    num_destroy(&jordan_data[3]);
}

static void test_number_matrix_functions(void)
{
    printf(C_CYAN "TEST: number_t matrix functions\n" C_RESET);

    number_t diag_real[2];
    number_t diag_complex[2];
    number_t jordan_upper_data[4];
    number_t jordan_lower_data[4];
    number_t jordan_log_data[4];
    number_t jordan_sqrt_data[4];
    number_t jordan_trig_data[4];
    number_t jordan_asin_data[4];
    number_t jordan_acos_data[4];
    number_t jordan_asinh_data[4];
    number_t jordan_acosh_data[4];
    number_t jordan_atan_data[4];
    number_t jordan_atanh_data[4];
    number_t expected;
    number_t got;
    matrix_t *A_real = NULL;
    matrix_t *A_complex = NULL;
    matrix_t *J_upper_exact = NULL;
    matrix_t *J_upper_hp = NULL;
    matrix_t *J_lower_hp = NULL;
    matrix_t *J_log = NULL;
    matrix_t *J_sqrt = NULL;
    matrix_t *J_trig = NULL;
    matrix_t *J_confluent = NULL;
    matrix_t *J_confluent_exp = NULL;
    matrix_t *J_asin = NULL;
    matrix_t *J_acos = NULL;
    matrix_t *J_asinh = NULL;
    matrix_t *J_acosh = NULL;
    matrix_t *J_atan = NULL;
    matrix_t *J_atanh = NULL;
    matrix_t *Jexp = NULL;
    matrix_t *E = NULL;
    matrix_t *L = NULL;
    matrix_t *R = NULL;
    matrix_t *S = NULL;
    matrix_t *C = NULL;
    matrix_t *T = NULL;
    matrix_t *SH = NULL;
    matrix_t *CH = NULL;
    matrix_t *TH = NULL;
    matrix_t *ASN = NULL;
    matrix_t *ACS = NULL;
    matrix_t *ASH = NULL;
    matrix_t *ACH = NULL;
    matrix_t *AT = NULL;
    matrix_t *ATH = NULL;

    diag_real[0] = num_create_from_string("1.25");
    diag_real[1] = num_create_from_string("2.5");
    num_set_prec_bits(&diag_real[0], 512u);
    num_set_prec_bits(&diag_real[1], 512u);

    A_real = mat_create_diagonal_num(2, diag_real);
    check_bool("mat_create_diagonal_num(real) not NULL", A_real != NULL);
    check_bool("mat_create_diagonal_num(real) -> MAT_TYPE_NUMBER",
               A_real != NULL && mat_typeof(A_real) == MAT_TYPE_NUMBER);

    {
        number_t pinv_diag[2] = {num_create_from_string("2"), num_create_from_string("4")};
        matrix_t *A_pinv = mat_create_diagonal_num(2, pinv_diag);
        matrix_t *P = mat_pseudoinverse(A_pinv);
        number_t expected00 = num_create_from_string("0.5");
        number_t expected11 = num_create_from_string("0.25");

        check_bool("mat_pseudoinverse(number diagonal) not NULL", P != NULL);
        check_bool("mat_pseudoinverse(number diagonal) -> MAT_TYPE_NUMBER",
                   P != NULL && mat_typeof(P) == MAT_TYPE_NUMBER);
        if (P) {
            number_t got00 = mat_get_num(P, 0, 0);
            number_t got11 = mat_get_num(P, 1, 1);
            check_bool("mat_pseudoinverse(number diagonal)[0,0] = 1/2", num_eq(got00, expected00));
            check_bool("mat_pseudoinverse(number diagonal)[1,1] = 1/4", num_eq(got11, expected11));
            num_destroy(&got00);
            num_destroy(&got11);
        }

        num_destroy(&expected00);
        num_destroy(&expected11);
        mat_free(P);
        mat_free(A_pinv);
        num_destroy(&pinv_diag[0]);
        num_destroy(&pinv_diag[1]);
    }

    E = mat_exp(A_real);
    L = mat_log(A_real);
    R = mat_sqrt(A_real);

    check_bool("mat_exp(number diagonal) -> MAT_TYPE_NUMBER", E != NULL && mat_typeof(E) == MAT_TYPE_NUMBER);
    check_bool("mat_log(number diagonal) -> MAT_TYPE_NUMBER", L != NULL && mat_typeof(L) == MAT_TYPE_NUMBER);
    check_bool("mat_sqrt(number diagonal) -> MAT_TYPE_NUMBER", R != NULL && mat_typeof(R) == MAT_TYPE_NUMBER);

    if (E) {
        got = mat_get_num(E, 0, 0);
        expected = num_exp(diag_real[0]);
        check_bool("mat_exp(number diagonal)[0,0] matches num_exp", num_eq(got, expected));
        check_bool("mat_exp(number diagonal)[0,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("exp(diag_real)[0,0]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);
    }

    if (L) {
        got = mat_get_num(L, 1, 1);
        expected = num_log(diag_real[1]);
        check_bool("mat_log(number diagonal)[1,1] matches num_log", num_eq(got, expected));
        check_bool("mat_log(number diagonal)[1,1] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("log(diag_real)[1,1]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);
    }

    if (R) {
        got = mat_get_num(R, 0, 0);
        expected = num_sqrt(diag_real[0]);
        check_bool("mat_sqrt(number diagonal)[0,0] matches num_sqrt", num_eq(got, expected));
        check_bool("mat_sqrt(number diagonal)[0,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("sqrt(diag_real)[0,0]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);
    }

    J_confluent = mat_from_string("(0, 1, 1; 0, 2, 1; 0, 0, 0)");
    check_bool("mat_from_string_expr(number confluent upper triangular) not NULL", J_confluent != NULL);

    J_confluent_exp = mat_exp(J_confluent);
    check_bool("mat_exp(number confluent upper triangular) not NULL", J_confluent_exp != NULL);
    check_bool("mat_exp(number confluent upper triangular) -> MAT_TYPE_NUMBER",
               J_confluent_exp != NULL && mat_typeof(J_confluent_exp) == MAT_TYPE_NUMBER);

    if (J_confluent_exp) {
        qcomplex_t expected00 = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t expected11 = qc_make(qf_from_double(exp(2.0)), QF_ZERO);
        qcomplex_t expected22 = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t expected01 = qc_make(qf_from_double((exp(2.0) - 1.0) / 2.0), QF_ZERO);
        qcomplex_t expected12 = qc_make(qf_from_double((exp(2.0) - 1.0) / 2.0), QF_ZERO);
        qcomplex_t expected02 = qc_make(qf_from_double((exp(2.0) + 1.0) / 4.0), QF_ZERO);
        number_t got00, got11, got22, got01, got12, got02;

        got00 = mat_get_num(J_confluent_exp, 0, 0);
        got11 = mat_get_num(J_confluent_exp, 1, 1);
        got22 = mat_get_num(J_confluent_exp, 2, 2);
        got01 = mat_get_num(J_confluent_exp, 0, 1);
        got12 = mat_get_num(J_confluent_exp, 1, 2);
        got02 = mat_get_num(J_confluent_exp, 0, 2);

        check_qc_val("exp(confluent upper triangular)[0,0] = 1", num_to_qcomplex(got00), expected00, 1e-10);
        check_qc_val("exp(confluent upper triangular)[1,1] = exp(2)", num_to_qcomplex(got11), expected11, 1e-10);
        check_qc_val("exp(confluent upper triangular)[2,2] = 1", num_to_qcomplex(got22), expected22, 1e-10);
        check_qc_val("exp(confluent upper triangular)[0,1]", num_to_qcomplex(got01), expected01, 1e-10);
        check_qc_val("exp(confluent upper triangular)[1,2]", num_to_qcomplex(got12), expected12, 1e-10);
        check_qc_val("exp(confluent upper triangular)[0,2]", num_to_qcomplex(got02), expected02, 1e-10);

        num_destroy(&got00);
        num_destroy(&got11);
        num_destroy(&got22);
        num_destroy(&got01);
        num_destroy(&got12);
        num_destroy(&got02);
    }

    diag_complex[0] = num_create_from_string("1 + 2i");
    diag_complex[1] = num_create_from_string("0");
    num_set_prec_bits(&diag_complex[0], 384u);

    A_complex = mat_create_diagonal_num(2, diag_complex);
    check_bool("mat_create_diagonal_num(complex) not NULL", A_complex != NULL);

    S = mat_sin(A_complex);
    check_bool("mat_sin(number complex diagonal) -> MAT_TYPE_NUMBER", S != NULL && mat_typeof(S) == MAT_TYPE_NUMBER);

    if (S) {
        got = mat_get_num(S, 0, 0);
        expected = num_sin(diag_complex[0]);
        check_bool("mat_sin(number complex diagonal)[0,0] matches num_sin", num_eq(got, expected));
        check_bool("mat_sin(number complex diagonal)[0,0] preserves precision", num_get_prec_bits(got) == 384u);
        print_matrix_precision_comparison("sin(diag_complex)[0,0]", got, expected);
        num_destroy(&expected);
        num_destroy(&got);
    }

    jordan_upper_data[0] = num_create_from_long(0);
    jordan_upper_data[1] = num_create_from_long(1);
    jordan_upper_data[2] = num_create_from_long(0);
    jordan_upper_data[3] = num_create_from_long(0);
    J_upper_exact = mat_create_num(2, 2, jordan_upper_data);
    check_bool("mat_create_num(number nilpotent upper Jordan) not NULL", J_upper_exact != NULL);
    Jexp = mat_exp(J_upper_exact);
    check_bool("mat_exp(number nilpotent upper Jordan) -> MAT_TYPE_NUMBER",
               Jexp != NULL && mat_typeof(Jexp) == MAT_TYPE_NUMBER);
    if (Jexp) {
        got = mat_get_num(Jexp, 0, 0);
        check_bool("exp(number nilpotent upper Jordan)[0,0] = 1", num_eq(got, NUM_ONE));
        num_destroy(&got);
        got = mat_get_num(Jexp, 0, 1);
        check_bool("exp(number nilpotent upper Jordan)[0,1] = 1", num_eq(got, NUM_ONE));
        num_destroy(&got);
        got = mat_get_num(Jexp, 1, 0);
        check_bool("exp(number nilpotent upper Jordan)[1,0] = 0", num_eq(got, NUM_ZERO));
        num_destroy(&got);
        got = mat_get_num(Jexp, 1, 1);
        check_bool("exp(number nilpotent upper Jordan)[1,1] = 1", num_eq(got, NUM_ONE));
        num_destroy(&got);
        check_bool("exp(number nilpotent upper Jordan) preserves upper-triangular structure",
                   mat_is_upper_triangular(Jexp));
    }
    mat_free(Jexp);
    Jexp = NULL;
    num_destroy(&jordan_upper_data[0]);
    num_destroy(&jordan_upper_data[1]);
    num_destroy(&jordan_upper_data[2]);
    num_destroy(&jordan_upper_data[3]);

    jordan_upper_data[0] = num_create_from_string("1.25");
    jordan_upper_data[1] = num_create_from_long(1);
    jordan_upper_data[2] = num_create_from_long(0);
    jordan_upper_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_upper_data[0], 512u);
    num_set_prec_bits(&jordan_upper_data[3], 512u);
    J_upper_hp = mat_create_num(2, 2, jordan_upper_data);
    check_bool("mat_create_num(number upper Jordan high precision) not NULL", J_upper_hp != NULL);
    Jexp = mat_exp(J_upper_hp);
    check_bool("mat_exp(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               Jexp != NULL && mat_typeof(Jexp) == MAT_TYPE_NUMBER);
    if (Jexp) {
        expected = num_exp(jordan_upper_data[0]);
        got = mat_get_num(Jexp, 0, 0);
        check_bool("exp(number upper Jordan)[0,0] matches exp(lambda)", num_eq(got, expected));
        check_bool("exp(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("exp(number upper Jordan)[0,0]", got, expected);
        num_destroy(&got);

        got = mat_get_num(Jexp, 0, 1);
        check_bool("exp(number upper Jordan)[0,1] matches exp(lambda)", num_eq(got, expected));
        check_bool("exp(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("exp(number upper Jordan)[0,1]", got, expected);
        num_destroy(&got);
        num_destroy(&expected);
        check_bool("exp(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(Jexp));
    }
    mat_free(Jexp);
    Jexp = NULL;
    num_destroy(&jordan_upper_data[0]);
    num_destroy(&jordan_upper_data[1]);
    num_destroy(&jordan_upper_data[2]);
    num_destroy(&jordan_upper_data[3]);

    jordan_lower_data[0] = num_create_from_string("1.25");
    jordan_lower_data[1] = num_create_from_long(0);
    jordan_lower_data[2] = num_create_from_long(1);
    jordan_lower_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_lower_data[0], 512u);
    num_set_prec_bits(&jordan_lower_data[3], 512u);
    J_lower_hp = mat_create_num(2, 2, jordan_lower_data);
    check_bool("mat_create_num(number lower Jordan high precision) not NULL", J_lower_hp != NULL);
    Jexp = mat_exp(J_lower_hp);
    check_bool("mat_exp(number lower Jordan high precision) -> MAT_TYPE_NUMBER",
               Jexp != NULL && mat_typeof(Jexp) == MAT_TYPE_NUMBER);
    if (Jexp) {
        expected = num_exp(jordan_lower_data[0]);
        got = mat_get_num(Jexp, 1, 0);
        check_bool("exp(number lower Jordan)[1,0] matches exp(lambda)", num_eq(got, expected));
        check_bool("exp(number lower Jordan)[1,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("exp(number lower Jordan)[1,0]", got, expected);
        num_destroy(&got);
        num_destroy(&expected);
        check_bool("exp(number lower Jordan) preserves lower-triangular structure", mat_is_lower_triangular(Jexp));
    }
    mat_free(L);
    L = NULL;
    mat_free(R);
    R = NULL;

    jordan_log_data[0] = num_create_from_string("1.25");
    jordan_log_data[1] = num_create_from_long(1);
    jordan_log_data[2] = num_create_from_long(0);
    jordan_log_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_log_data[0], 512u);
    num_set_prec_bits(&jordan_log_data[3], 512u);
    J_log = mat_create_num(2, 2, jordan_log_data);
    check_bool("mat_create_num(number upper Jordan for log) not NULL", J_log != NULL);
    L = mat_log(J_log);
    check_bool("mat_log(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               L != NULL && mat_typeof(L) == MAT_TYPE_NUMBER);
    if (L) {
        expected = num_log(jordan_log_data[0]);
        got = mat_get_num(L, 0, 0);
        check_bool("log(number upper Jordan)[0,0] matches log(lambda)", num_eq(got, expected));
        check_bool("log(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("log(number upper Jordan)[0,0]", got, expected);
        num_destroy(&got);
        num_destroy(&expected);

        expected = num_inv(jordan_log_data[0]);
        got = mat_get_num(L, 0, 1);
        check_bool("log(number upper Jordan)[0,1] matches 1/lambda", num_eq(got, expected));
        check_bool("log(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("log(number upper Jordan)[0,1]", got, expected);
        num_destroy(&got);
        num_destroy(&expected);
        check_bool("log(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(L));
    }

    jordan_sqrt_data[0] = num_create_from_string("1.25");
    jordan_sqrt_data[1] = num_create_from_long(1);
    jordan_sqrt_data[2] = num_create_from_long(0);
    jordan_sqrt_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_sqrt_data[0], 512u);
    num_set_prec_bits(&jordan_sqrt_data[3], 512u);
    J_sqrt = mat_create_num(2, 2, jordan_sqrt_data);
    check_bool("mat_create_num(number upper Jordan for sqrt) not NULL", J_sqrt != NULL);
    R = mat_sqrt(J_sqrt);
    check_bool("mat_sqrt(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               R != NULL && mat_typeof(R) == MAT_TYPE_NUMBER);
    if (R) {
        number_t two = num_create_from_long(2);

        expected = num_sqrt(jordan_sqrt_data[0]);
        got = mat_get_num(R, 0, 0);
        check_bool("sqrt(number upper Jordan)[0,0] matches sqrt(lambda)", num_eq(got, expected));
        check_bool("sqrt(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) == 512u);
        print_matrix_precision_comparison("sqrt(number upper Jordan)[0,0]", got, expected);
        num_destroy(&got);

        {
            number_t denom = num_mul(two, expected);
            number_t offdiag_expected = num_inv(denom);
            number_t offdiag_error;
            number_t offdiag_tolerance = num_create_from_string("1e-150");

            got = mat_get_num(R, 0, 1);
            offdiag_error = matrix_number_error_magnitude(got, offdiag_expected);
            check_bool("sqrt(number upper Jordan)[0,1] matches 1/(2*sqrt(lambda))",
                       num_le(offdiag_error, offdiag_tolerance));
            check_bool("sqrt(number upper Jordan)[0,1] does not lose precision", num_get_prec_bits(got) >= 512u);
            print_matrix_precision_comparison("sqrt(number upper Jordan)[0,1]", got, offdiag_expected);
            num_destroy(&offdiag_tolerance);
            num_destroy(&offdiag_error);
            num_destroy(&got);
            num_destroy(&offdiag_expected);
            num_destroy(&denom);
        }

        num_destroy(&expected);
        num_destroy(&two);
        check_bool("sqrt(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(R));
    }

    jordan_trig_data[0] = num_create_from_string("1.25");
    jordan_trig_data[1] = num_create_from_long(1);
    jordan_trig_data[2] = num_create_from_long(0);
    jordan_trig_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_trig_data[0], 512u);
    num_set_prec_bits(&jordan_trig_data[3], 512u);
    J_trig = mat_create_num(2, 2, jordan_trig_data);
    check_bool("mat_create_num(number upper Jordan for trig/hyperbolic) not NULL", J_trig != NULL);

    mat_free(S);
    S = NULL;
    S = mat_sin(J_trig);
    C = mat_cos(J_trig);
    T = mat_tan(J_trig);
    SH = mat_sinh(J_trig);
    CH = mat_cosh(J_trig);
    TH = mat_tanh(J_trig);

    check_bool("mat_sin(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               S != NULL && mat_typeof(S) == MAT_TYPE_NUMBER);
    check_bool("mat_cos(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               C != NULL && mat_typeof(C) == MAT_TYPE_NUMBER);
    check_bool("mat_tan(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               T != NULL && mat_typeof(T) == MAT_TYPE_NUMBER);
    check_bool("mat_sinh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               SH != NULL && mat_typeof(SH) == MAT_TYPE_NUMBER);
    check_bool("mat_cosh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               CH != NULL && mat_typeof(CH) == MAT_TYPE_NUMBER);
    check_bool("mat_tanh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               TH != NULL && mat_typeof(TH) == MAT_TYPE_NUMBER);

    if (S && C && T && SH && CH && TH) {
        number_t tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_trig_data[0]);
        number_t sin_lambda = num_sin(lambda);
        number_t cos_lambda = num_cos(lambda);
        number_t tan_lambda = num_tan(lambda);
        number_t sinh_lambda = num_sinh(lambda);
        number_t cosh_lambda = num_cosh(lambda);
        number_t tanh_lambda = num_tanh(lambda);
        number_t cos_sq = num_mul(cos_lambda, cos_lambda);
        number_t cosh_sq = num_mul(cosh_lambda, cosh_lambda);
        number_t sec2 = num_inv(cos_sq);
        number_t sech2 = num_inv(cosh_sq);
        number_t neg_sin_lambda = num_neg(sin_lambda);
        number_t err;

        got = mat_get_num(S, 0, 0);
        err = matrix_number_error_magnitude(got, sin_lambda);
        check_bool("sin(number upper Jordan)[0,0] matches sin(lambda)", num_le(err, tol));
        check_bool("sin(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("sin(number upper Jordan)[0,0]", got, sin_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(S, 0, 1);
        err = matrix_number_error_magnitude(got, cos_lambda);
        check_bool("sin(number upper Jordan)[0,1] matches cos(lambda)", num_le(err, tol));
        check_bool("sin(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("sin(number upper Jordan)[0,1]", got, cos_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(C, 0, 0);
        err = matrix_number_error_magnitude(got, cos_lambda);
        check_bool("cos(number upper Jordan)[0,0] matches cos(lambda)", num_le(err, tol));
        check_bool("cos(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("cos(number upper Jordan)[0,0]", got, cos_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(C, 0, 1);
        err = matrix_number_error_magnitude(got, neg_sin_lambda);
        check_bool("cos(number upper Jordan)[0,1] matches -sin(lambda)", num_le(err, tol));
        check_bool("cos(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("cos(number upper Jordan)[0,1]", got, neg_sin_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(T, 0, 0);
        err = matrix_number_error_magnitude(got, tan_lambda);
        check_bool("tan(number upper Jordan)[0,0] matches tan(lambda)", num_le(err, tol));
        check_bool("tan(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("tan(number upper Jordan)[0,0]", got, tan_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(T, 0, 1);
        err = matrix_number_error_magnitude(got, sec2);
        check_bool("tan(number upper Jordan)[0,1] matches sec(lambda)^2", num_le(err, tol));
        check_bool("tan(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("tan(number upper Jordan)[0,1]", got, sec2);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(SH, 0, 0);
        err = matrix_number_error_magnitude(got, sinh_lambda);
        check_bool("sinh(number upper Jordan)[0,0] matches sinh(lambda)", num_le(err, tol));
        check_bool("sinh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("sinh(number upper Jordan)[0,0]", got, sinh_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(SH, 0, 1);
        err = matrix_number_error_magnitude(got, cosh_lambda);
        check_bool("sinh(number upper Jordan)[0,1] matches cosh(lambda)", num_le(err, tol));
        check_bool("sinh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("sinh(number upper Jordan)[0,1]", got, cosh_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(CH, 0, 0);
        err = matrix_number_error_magnitude(got, cosh_lambda);
        check_bool("cosh(number upper Jordan)[0,0] matches cosh(lambda)", num_le(err, tol));
        check_bool("cosh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("cosh(number upper Jordan)[0,0]", got, cosh_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(CH, 0, 1);
        err = matrix_number_error_magnitude(got, sinh_lambda);
        check_bool("cosh(number upper Jordan)[0,1] matches sinh(lambda)", num_le(err, tol));
        check_bool("cosh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("cosh(number upper Jordan)[0,1]", got, sinh_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(TH, 0, 0);
        err = matrix_number_error_magnitude(got, tanh_lambda);
        check_bool("tanh(number upper Jordan)[0,0] matches tanh(lambda)", num_le(err, tol));
        check_bool("tanh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("tanh(number upper Jordan)[0,0]", got, tanh_lambda);
        num_destroy(&err);
        num_destroy(&got);

        got = mat_get_num(TH, 0, 1);
        err = matrix_number_error_magnitude(got, sech2);
        check_bool("tanh(number upper Jordan)[0,1] matches sech(lambda)^2", num_le(err, tol));
        check_bool("tanh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("tanh(number upper Jordan)[0,1]", got, sech2);
        num_destroy(&err);
        num_destroy(&got);

        check_bool("sin(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(S));
        check_bool("cos(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(C));
        check_bool("tan(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(T));
        check_bool("sinh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(SH));
        check_bool("cosh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(CH));
        check_bool("tanh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(TH));

        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&sin_lambda);
        num_destroy(&cos_lambda);
        num_destroy(&tan_lambda);
        num_destroy(&sinh_lambda);
        num_destroy(&cosh_lambda);
        num_destroy(&tanh_lambda);
        num_destroy(&cos_sq);
        num_destroy(&cosh_sq);
        num_destroy(&sec2);
        num_destroy(&sech2);
        num_destroy(&neg_sin_lambda);
    }

    jordan_asin_data[0] = num_create_from_string("0.25");
    jordan_asin_data[1] = num_create_from_long(1);
    jordan_asin_data[2] = num_create_from_long(0);
    jordan_asin_data[3] = num_create_from_string("0.25");
    num_set_prec_bits(&jordan_asin_data[0], 512u);
    num_set_prec_bits(&jordan_asin_data[3], 512u);
    J_asin = mat_create_num(2, 2, jordan_asin_data);
    check_bool("mat_create_num(number upper Jordan for asin) not NULL", J_asin != NULL);
    ASN = mat_asin(J_asin);
    check_bool("mat_asin(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               ASN != NULL && mat_typeof(ASN) == MAT_TYPE_NUMBER);
    if (ASN) {
        number_t tol = num_create_from_string("1e-90");
        number_t imag_tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_asin_data[0]);
        number_t lambda_sq = num_mul(lambda, lambda);
        number_t inside = num_sub(NUM_ONE, lambda_sq);
        number_t root = num_sqrt(inside);
        number_t deriv_expected = num_inv(root);
        number_t err;

        num_destroy(&root);

        expected = num_asin(lambda);
        got = mat_get_num(ASN, 0, 0);
        {
            number_t got_real = num_real_part(got);
            number_t got_imag = num_imag_part(got);
            number_t imag_err = num_abs(got_imag);

            err = matrix_number_error_magnitude(got_real, expected);
            check_bool("asin(number upper Jordan)[0,0] has negligible imaginary part", num_le(imag_err, imag_tol));
            num_destroy(&imag_err);
            num_destroy(&got_imag);
            num_destroy(&got_real);
        }
        check_bool("asin(number upper Jordan)[0,0] matches asin(lambda)", num_le(err, tol));
        check_bool("asin(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("asin(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(ASN, 0, 1);
        err = matrix_number_error_magnitude(got, deriv_expected);
        check_bool("asin(number upper Jordan)[0,1] matches 1/sqrt(1-lambda^2)", num_le(err, tol));
        check_bool("asin(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("asin(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("asin(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(ASN));

        num_destroy(&imag_tol);
        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lambda_sq);
        num_destroy(&inside);
        num_destroy(&deriv_expected);
    }

    jordan_acos_data[0] = num_create_from_string("0.25");
    jordan_acos_data[1] = num_create_from_long(1);
    jordan_acos_data[2] = num_create_from_long(0);
    jordan_acos_data[3] = num_create_from_string("0.25");
    num_set_prec_bits(&jordan_acos_data[0], 512u);
    num_set_prec_bits(&jordan_acos_data[3], 512u);
    J_acos = mat_create_num(2, 2, jordan_acos_data);
    check_bool("mat_create_num(number upper Jordan for acos) not NULL", J_acos != NULL);
    ACS = mat_acos(J_acos);
    check_bool("mat_acos(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               ACS != NULL && mat_typeof(ACS) == MAT_TYPE_NUMBER);
    if (ACS) {
        number_t tol = num_create_from_string("1e-90");
        number_t imag_tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_acos_data[0]);
        number_t lambda_sq = num_mul(lambda, lambda);
        number_t inside = num_sub(NUM_ONE, lambda_sq);
        number_t root = num_sqrt(inside);
        number_t inverse = num_inv(root);
        number_t deriv_expected = num_neg(inverse);
        number_t err;

        num_destroy(&inverse);
        num_destroy(&root);

        expected = num_acos(lambda);
        got = mat_get_num(ACS, 0, 0);
        {
            number_t got_real = num_real_part(got);
            number_t got_imag = num_imag_part(got);
            number_t imag_err = num_abs(got_imag);

            err = matrix_number_error_magnitude(got_real, expected);
            check_bool("acos(number upper Jordan)[0,0] has negligible imaginary part", num_le(imag_err, imag_tol));
            num_destroy(&imag_err);
            num_destroy(&got_imag);
            num_destroy(&got_real);
        }
        check_bool("acos(number upper Jordan)[0,0] matches acos(lambda)", num_le(err, tol));
        check_bool("acos(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("acos(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(ACS, 0, 1);
        err = matrix_number_error_magnitude(got, deriv_expected);
        check_bool("acos(number upper Jordan)[0,1] matches -1/sqrt(1-lambda^2)", num_le(err, tol));
        check_bool("acos(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("acos(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("acos(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(ACS));

        num_destroy(&imag_tol);
        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lambda_sq);
        num_destroy(&inside);
        num_destroy(&deriv_expected);
    }

    jordan_asinh_data[0] = num_create_from_string("1.25");
    jordan_asinh_data[1] = num_create_from_long(1);
    jordan_asinh_data[2] = num_create_from_long(0);
    jordan_asinh_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_asinh_data[0], 512u);
    num_set_prec_bits(&jordan_asinh_data[3], 512u);
    J_asinh = mat_create_num(2, 2, jordan_asinh_data);
    check_bool("mat_create_num(number upper Jordan for asinh) not NULL", J_asinh != NULL);
    ASH = mat_asinh(J_asinh);
    check_bool("mat_asinh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               ASH != NULL && mat_typeof(ASH) == MAT_TYPE_NUMBER);
    if (ASH) {
        number_t tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_asinh_data[0]);
        number_t lambda_sq = num_mul(lambda, lambda);
        number_t inside = num_add(lambda_sq, NUM_ONE);
        number_t root = num_sqrt(inside);
        number_t deriv_expected = num_inv(root);
        number_t err;

        num_destroy(&root);

        expected = num_asinh(lambda);
        got = mat_get_num(ASH, 0, 0);
        err = matrix_number_error_magnitude(got, expected);
        check_bool("asinh(number upper Jordan)[0,0] matches asinh(lambda)", num_le(err, tol));
        check_bool("asinh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("asinh(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(ASH, 0, 1);
        err = matrix_number_error_magnitude(got, deriv_expected);
        check_bool("asinh(number upper Jordan)[0,1] matches 1/sqrt(1+lambda^2)", num_le(err, tol));
        check_bool("asinh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("asinh(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("asinh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(ASH));

        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lambda_sq);
        num_destroy(&inside);
        num_destroy(&deriv_expected);
    }

    jordan_acosh_data[0] = num_create_from_string("1.25");
    jordan_acosh_data[1] = num_create_from_long(1);
    jordan_acosh_data[2] = num_create_from_long(0);
    jordan_acosh_data[3] = num_create_from_string("1.25");
    num_set_prec_bits(&jordan_acosh_data[0], 512u);
    num_set_prec_bits(&jordan_acosh_data[3], 512u);
    J_acosh = mat_create_num(2, 2, jordan_acosh_data);
    check_bool("mat_create_num(number upper Jordan for acosh) not NULL", J_acosh != NULL);
    ACH = mat_acosh(J_acosh);
    check_bool("mat_acosh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               ACH != NULL && mat_typeof(ACH) == MAT_TYPE_NUMBER);
    if (ACH) {
        number_t tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_acosh_data[0]);
        number_t lm1 = num_sub(lambda, NUM_ONE);
        number_t lp1 = num_add(lambda, NUM_ONE);
        number_t left_root = num_sqrt(lm1);
        number_t right_root = num_sqrt(lp1);
        number_t root_product = num_mul(left_root, right_root);
        number_t deriv_expected = num_inv(root_product);
        number_t err;

        num_destroy(&root_product);
        num_destroy(&right_root);
        num_destroy(&left_root);

        expected = num_acosh(lambda);
        got = mat_get_num(ACH, 0, 0);
        err = matrix_number_error_magnitude(got, expected);
        check_bool("acosh(number upper Jordan)[0,0] matches acosh(lambda)", num_le(err, tol));
        check_bool("acosh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("acosh(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(ACH, 0, 1);
        err = matrix_number_error_magnitude(got, deriv_expected);
        check_bool("acosh(number upper Jordan)[0,1] matches 1/(sqrt(lambda-1)*sqrt(lambda+1))", num_le(err, tol));
        check_bool("acosh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("acosh(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("acosh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(ACH));

        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lm1);
        num_destroy(&lp1);
        num_destroy(&deriv_expected);
    }

    jordan_atan_data[0] = num_create_from_string("0.25");
    jordan_atan_data[1] = num_create_from_long(1);
    jordan_atan_data[2] = num_create_from_long(0);
    jordan_atan_data[3] = num_create_from_string("0.25");
    num_set_prec_bits(&jordan_atan_data[0], 512u);
    num_set_prec_bits(&jordan_atan_data[3], 512u);
    J_atan = mat_create_num(2, 2, jordan_atan_data);
    check_bool("mat_create_num(number upper Jordan for atan) not NULL", J_atan != NULL);
    AT = mat_atan(J_atan);
    check_bool("mat_atan(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               AT != NULL && mat_typeof(AT) == MAT_TYPE_NUMBER);
    if (AT) {
        number_t tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_atan_data[0]);
        number_t lambda_sq = num_mul(lambda, lambda);
        number_t denom = num_add(NUM_ONE, lambda_sq);
        number_t deriv_expected = num_inv(denom);
        number_t err;

        expected = num_atan(lambda);
        got = mat_get_num(AT, 0, 0);
        err = matrix_number_error_magnitude(got, expected);
        check_bool("atan(number upper Jordan)[0,0] matches atan(lambda)", num_le(err, tol));
        check_bool("atan(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("atan(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(AT, 0, 1);
        {
            number_t got_real = num_real_part(got);
            number_t got_imag = num_imag_part(got);
            number_t imag_err = num_abs(got_imag);

            err = matrix_number_error_magnitude(got_real, deriv_expected);
            check_bool("atan(number upper Jordan)[0,1] has negligible imaginary part", num_le(imag_err, tol));
            num_destroy(&imag_err);
            num_destroy(&got_imag);
            num_destroy(&got_real);
        }
        check_bool("atan(number upper Jordan)[0,1] matches 1/(1+lambda^2)", num_le(err, tol));
        check_bool("atan(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("atan(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("atan(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(AT));

        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lambda_sq);
        num_destroy(&denom);
        num_destroy(&deriv_expected);
    }

    jordan_atanh_data[0] = num_create_from_string("0.25");
    jordan_atanh_data[1] = num_create_from_long(1);
    jordan_atanh_data[2] = num_create_from_long(0);
    jordan_atanh_data[3] = num_create_from_string("0.25");
    num_set_prec_bits(&jordan_atanh_data[0], 512u);
    num_set_prec_bits(&jordan_atanh_data[3], 512u);
    J_atanh = mat_create_num(2, 2, jordan_atanh_data);
    check_bool("mat_create_num(number upper Jordan for atanh) not NULL", J_atanh != NULL);
    ATH = mat_atanh(J_atanh);
    check_bool("mat_atanh(number upper Jordan high precision) -> MAT_TYPE_NUMBER",
               ATH != NULL && mat_typeof(ATH) == MAT_TYPE_NUMBER);
    if (ATH) {
        number_t tol = num_create_from_string("1e-90");
        number_t lambda = num_clone(jordan_atanh_data[0]);
        number_t lambda_sq = num_mul(lambda, lambda);
        number_t denom = num_sub(NUM_ONE, lambda_sq);
        number_t deriv_expected = num_inv(denom);
        number_t err;

        expected = num_atanh(lambda);
        got = mat_get_num(ATH, 0, 0);
        err = matrix_number_error_magnitude(got, expected);
        check_bool("atanh(number upper Jordan)[0,0] matches atanh(lambda)", num_le(err, tol));
        check_bool("atanh(number upper Jordan)[0,0] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("atanh(number upper Jordan)[0,0]", got, expected);
        num_destroy(&err);
        num_destroy(&got);
        num_destroy(&expected);

        got = mat_get_num(ATH, 0, 1);
        err = matrix_number_error_magnitude(got, deriv_expected);
        check_bool("atanh(number upper Jordan)[0,1] matches 1/(1-lambda^2)", num_le(err, tol));
        check_bool("atanh(number upper Jordan)[0,1] preserves precision", num_get_prec_bits(got) >= 512u);
        print_matrix_precision_comparison("atanh(number upper Jordan)[0,1]", got, deriv_expected);
        num_destroy(&err);
        num_destroy(&got);
        check_bool("atanh(number upper Jordan) preserves upper-triangular structure", mat_is_upper_triangular(ATH));

        num_destroy(&tol);
        num_destroy(&lambda);
        num_destroy(&lambda_sq);
        num_destroy(&denom);
        num_destroy(&deriv_expected);
    }

    check_number_upper_jordan_from_expr("gamma(number upper Jordan)[0,0]", mat_gamma, expr_gamma, "2.5", 512u, "1e-90");
    check_number_upper_jordan_from_expr("erfinv(number upper Jordan)[0,0]", mat_erfinv, expr_erfinv, "0.5", 512u,
                                        "1e-90");
    check_number_upper_jordan_from_expr("erfcinv(number upper Jordan)[0,0]", mat_erfcinv, expr_erfcinv, "0.4", 512u,
                                        "1e-90");
    check_number_upper_jordan_from_expr("lgamma(number upper Jordan)[0,0]", mat_lgamma, expr_lgamma, "2.5", 512u,
                                        "1e-90");
    check_number_upper_jordan_from_expr("digamma(number upper Jordan)[0,0]", mat_digamma, expr_digamma, "2.5", 512u,
                                        "1e-90");
    check_number_upper_jordan_from_expr("trigamma(number upper Jordan)[0,0]", mat_trigamma, expr_trigamma, "2.5", 512u,
                                        "1e-90");
    check_number_upper_jordan_from_expr("gammainv(number upper Jordan)[0,0]", mat_gammainv, expr_gammainv,
                                        GAMMAINV_HP_INPUT_TEXT, 512u, "1e-90");
    check_number_upper_jordan_from_expr("lambert_w0(number upper Jordan)[0,0]", mat_lambert_w0, expr_lambert_w0, "0.2",
                                        512u, "1e-90");
    check_number_upper_jordan_from_expr("productlog(number upper Jordan)[0,0]", mat_productlog, expr_lambert_w0, "0.2",
                                        512u, "1e-90");
    check_number_upper_jordan_from_expr("lambert_wm1(number upper Jordan)[0,0]", mat_lambert_wm1, expr_lambert_wm1,
                                        "-0.2", 512u, "1e-30");
    check_number_upper_jordan_from_expr("ei(number upper Jordan)[0,0]", mat_Ei, expr_Ei, "0.5", 512u, "1e-90");
    check_number_upper_jordan_from_expr("e1(number upper Jordan)[0,0]", mat_E1, expr_E1, "1.0", 512u, "1e-90");

    mat_free(A_real);
    mat_free(A_complex);
    mat_free(J_upper_exact);
    mat_free(J_upper_hp);
    mat_free(J_lower_hp);
    mat_free(J_confluent_exp);
    mat_free(J_confluent);
    mat_free(J_log);
    mat_free(J_sqrt);
    mat_free(J_trig);
    mat_free(J_asin);
    mat_free(J_acos);
    mat_free(J_asinh);
    mat_free(J_acosh);
    mat_free(J_atan);
    mat_free(J_atanh);
    mat_free(Jexp);
    mat_free(E);
    mat_free(L);
    mat_free(R);
    mat_free(S);
    mat_free(C);
    mat_free(T);
    mat_free(SH);
    mat_free(CH);
    mat_free(TH);
    mat_free(ASN);
    mat_free(ACS);
    mat_free(ASH);
    mat_free(ACH);
    mat_free(AT);
    mat_free(ATH);
    num_destroy(&diag_real[0]);
    num_destroy(&diag_real[1]);
    num_destroy(&diag_complex[0]);
    num_destroy(&diag_complex[1]);
    num_destroy(&jordan_lower_data[0]);
    num_destroy(&jordan_lower_data[1]);
    num_destroy(&jordan_lower_data[2]);
    num_destroy(&jordan_lower_data[3]);
    num_destroy(&jordan_log_data[0]);
    num_destroy(&jordan_log_data[1]);
    num_destroy(&jordan_log_data[2]);
    num_destroy(&jordan_log_data[3]);
    num_destroy(&jordan_sqrt_data[0]);
    num_destroy(&jordan_sqrt_data[1]);
    num_destroy(&jordan_sqrt_data[2]);
    num_destroy(&jordan_sqrt_data[3]);
    num_destroy(&jordan_trig_data[0]);
    num_destroy(&jordan_trig_data[1]);
    num_destroy(&jordan_trig_data[2]);
    num_destroy(&jordan_trig_data[3]);
    num_destroy(&jordan_asin_data[0]);
    num_destroy(&jordan_asin_data[1]);
    num_destroy(&jordan_asin_data[2]);
    num_destroy(&jordan_asin_data[3]);
    num_destroy(&jordan_acos_data[0]);
    num_destroy(&jordan_acos_data[1]);
    num_destroy(&jordan_acos_data[2]);
    num_destroy(&jordan_acos_data[3]);
    num_destroy(&jordan_asinh_data[0]);
    num_destroy(&jordan_asinh_data[1]);
    num_destroy(&jordan_asinh_data[2]);
    num_destroy(&jordan_asinh_data[3]);
    num_destroy(&jordan_acosh_data[0]);
    num_destroy(&jordan_acosh_data[1]);
    num_destroy(&jordan_acosh_data[2]);
    num_destroy(&jordan_acosh_data[3]);
    num_destroy(&jordan_atan_data[0]);
    num_destroy(&jordan_atan_data[1]);
    num_destroy(&jordan_atan_data[2]);
    num_destroy(&jordan_atan_data[3]);
    num_destroy(&jordan_atanh_data[0]);
    num_destroy(&jordan_atanh_data[1]);
    num_destroy(&jordan_atanh_data[2]);
    num_destroy(&jordan_atanh_data[3]);
}

static void check_expr_expr_contains(const char *label, expr_t *dv, const char *needle)
{
    char *s = expr_to_string(dv, style_EXPRESSION);
    check_bool(label, s != NULL && strstr(s, needle) != NULL);
    free(s);
}

static void test_expr_matrix_functions(void)
{
    printf(C_CYAN "TEST: expr matrix functions\n" C_RESET);

    expr_t *x = test_expr_new_named_var_d(2.0, "x");
    expr_t *one = test_expr_new_const_d(1.0);

    {
        expr_t *diag_vals[4] = {x, EXPR_ZERO, EXPR_ZERO, one};
        matrix_t *A = mat_create_expr(2, 2, diag_vals);
        matrix_t *E = mat_exp(A);
        expr_t *e00 = NULL;
        expr_t *e11 = NULL;

        check_bool("mat_exp(expr diagonal) not NULL", E != NULL);
        print_mdv("A", A);
        if (E) {
            print_mdv("exp(A)", E);
            mat_get(E, 0, 0, &e00);
            mat_get(E, 1, 1, &e11);
            check_d("exp(expr diag)[0,0] = exp(2)", expr_eval_d(e00), exp(2.0), 1e-12);
            check_d("exp(expr diag)[1,1] = exp(1)", expr_eval_d(e11), exp(1.0), 1e-12);
            test_expr_set_val_d(x, 3.0);
            check_d("exp(expr diag)[0,0] tracks x", expr_eval_d(e00), exp(3.0), 1e-12);
        }

        mat_free(A);
        mat_free(E);
    }

    test_expr_set_val_d(x, 2.0);

    {
        expr_t *tri_vals[4] = {x, one, EXPR_ZERO, x};
        matrix_t *T = mat_create_expr(2, 2, tri_vals);
        matrix_t *E = mat_exp(T);
        expr_t *e00 = NULL;
        expr_t *e01 = NULL;
        expr_t *e11 = NULL;

        check_bool("mat_exp(expr Jordan block) not NULL", E != NULL);
        print_mdv("T", T);
        if (E) {
            print_mdv("exp(T)", E);
            mat_get(E, 0, 0, &e00);
            mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 1, &e11);
            check_d("exp([[x,1],[0,x]])[0,0] = exp(2)", expr_eval_d(e00), exp(2.0), 1e-12);
            check_d("exp([[x,1],[0,x]])[0,1] = exp(2)", expr_eval_d(e01), exp(2.0), 1e-12);
            check_d("exp([[x,1],[0,x]])[1,1] = exp(2)", expr_eval_d(e11), exp(2.0), 1e-12);
            test_expr_set_val_d(x, 3.0);
            check_d("Jordan exp tracks x on diagonal", expr_eval_d(e00), exp(3.0), 1e-12);
            check_d("Jordan exp tracks x on superdiag", expr_eval_d(e01), exp(3.0), 1e-12);
        }

        mat_free(T);
        mat_free(E);
    }

    {
        expr_t *dense_vals[4] = {x, one, one, x};
        matrix_t *A = mat_create_expr(2, 2, dense_vals);
        matrix_t *E = mat_exp(A);
        matrix_t *L = mat_log(A);
        matrix_t *S = mat_sin(A);
        matrix_t *Ai = mat_inverse(A);
        int rank = mat_rank(A);
        expr_t *v = NULL;

        test_expr_set_val_d(x, 2.0);
        print_mdv("A", A);
        check_bool("mat_exp(expr dense 2x2 diagonalizable) not NULL", E != NULL);
        check_bool("mat_log(expr dense 2x2 diagonalizable) not NULL", L != NULL);
        check_bool("mat_sin(expr dense 2x2 diagonalizable) not NULL", S != NULL);
        check_bool("mat_inverse(expr 2x2) now supported", Ai != NULL);
        if (Ai)
            print_mdv("A^{-1}", Ai);
        check_bool("mat_rank(expr 2x2) = 2", rank == 2);
        if (E) {
            print_mdv("exp(A)", E);
            mat_get(E, 0, 0, &v);
            check_d("exp(expr dense 2x2)[0,0]", expr_eval_d(v), exp(2.0) * cosh(1.0), 1e-12);
            check_expr_expr_contains("exp(expr dense 2x2)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(expr dense 2x2)[0,1]", expr_eval_d(v), exp(2.0) * sinh(1.0), 1e-12);
        }
        if (L) {
            print_mdv("log(A)", L);
            mat_get(L, 0, 0, &v);
            check_d("log(expr dense 2x2)[0,0]", expr_eval_d(v), 0.5 * (log(3.0) + log(1.0)), 1e-12);
            check_expr_expr_contains("log(expr dense 2x2)[0,0] stays symbolic", v, "ln");
            mat_get(L, 0, 1, &v);
            check_d("log(expr dense 2x2)[0,1]", expr_eval_d(v), 0.5 * (log(3.0) - log(1.0)), 1e-12);
        }
        if (S) {
            print_mdv("sin(A)", S);
            mat_get(S, 0, 0, &v);
            check_d("sin(expr dense 2x2)[0,0]", expr_eval_d(v), sin(2.0) * cos(1.0), 1e-12);
            check_expr_expr_contains("sin(expr dense 2x2)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 1, &v);
            check_d("sin(expr dense 2x2)[0,1]", expr_eval_d(v), cos(2.0) * sin(1.0), 1e-12);
        }

        test_expr_set_val_d(x, 3.0);
        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(expr dense 2x2)[0,0] tracks x", expr_eval_d(v), exp(3.0) * cosh(1.0), 1e-12);
        }
        if (L) {
            mat_get(L, 0, 1, &v);
            check_d("log(expr dense 2x2)[0,1] tracks x", expr_eval_d(v), 0.5 * (log(4.0) - log(2.0)), 1e-12);
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_d("sin(expr dense 2x2)[0,1] tracks x", expr_eval_d(v), cos(3.0) * sin(1.0), 1e-12);
        }

        mat_free(Ai);
        mat_free(A);
        mat_free(E);
        mat_free(L);
        mat_free(S);
    }

    expr_free(one);
    expr_free(x);
}

static void test_expr_matrix_functions_extended(void)
{
    printf(C_CYAN "TEST: expr matrix functions (extended symbolic coverage)\n" C_RESET);

    {
        expr_t *x = test_expr_new_named_var_d(0.2, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, EXPR_ZERO, EXPR_ZERO, one, x, EXPR_ZERO, EXPR_ZERO, one, x};
        matrix_t *T = mat_create_expr(3, 3, vals);
        matrix_t *R = NULL;
        matrix_t *G = NULL;
        matrix_t *W = NULL;
        expr_t *v = NULL;

        R = mat_erf(T);
        G = mat_gamma(T);
        W = mat_lambert_w0(T);

        check_bool("mat_erf(expr 3x3 lower Jordan) not NULL", R != NULL);
        check_bool("mat_gamma(expr 3x3 lower Jordan) not NULL", G != NULL);
        check_bool("mat_lambert_w0(expr 3x3 lower Jordan) not NULL", W != NULL);

        if (T)
            print_mdv("T", T);
        if (R)
            print_mdv("erf(T)", R);
        if (G)
            print_mdv("gamma(T)", G);
        if (W)
            print_mdv("lambert_w0(T)", W);

        if (R) {
            check_bool("erf(T) preserves lower-triangular structure", mat_is_lower_triangular(R));
            mat_get(R, 0, 0, &v);
            check_expr_expr_contains("erf(T)[0,0] stays symbolic in x", v, "erf(x)");
            test_expr_set_val_d(x, 0.3);
            check_d("erf(T)[0,0] tracks x", expr_eval_d(v), erf(0.3), 1e-12);
        }

        if (G) {
            check_bool("gamma(T) preserves lower-triangular structure", mat_is_lower_triangular(G));
            mat_get(G, 0, 0, &v);
            test_expr_set_val_d(x, 3.0);
            check_d("gamma(T)[0,0] tracks x", expr_eval_d(v), tgamma(3.0), 1e-12);
        }

        if (W) {
            check_bool("lambert_w0(T) preserves lower-triangular structure", mat_is_lower_triangular(W));
            mat_get(W, 0, 0, &v);
            check_expr_expr_contains("lambert_w0(T)[0,0] stays symbolic in x", v, "W₀");
            test_expr_set_val_d(x, 0.1);
            check_bool("lambert_w0(T)[0,0] numerically finite", isfinite(expr_eval_d(v)));
        }

        mat_free(T);
        mat_free(R);
        mat_free(G);
        mat_free(W);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(1.5, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *vals[9] = {x, EXPR_ZERO, EXPR_ZERO, one, x, EXPR_ZERO, EXPR_ZERO, one, x};
        matrix_t *T = mat_create_expr(3, 3, vals);
        matrix_t *E = mat_exp(T);
        expr_t *v = NULL;

        check_bool("mat_exp(expr 3x3 lower Jordan) not NULL", E != NULL);
        if (T)
            print_mdv("T", T);
        if (E)
            print_mdv("exp(T)", E);

        if (E) {
            check_bool("exp(lower Jordan) preserves lower-triangular structure", mat_is_lower_triangular(E));
            mat_get(E, 0, 0, &v);
            check_d("exp(lower Jordan)[0,0] = exp(1.5)", expr_eval_d(v), exp(1.5), 1e-12);
            mat_get(E, 1, 0, &v);
            check_d("exp(lower Jordan)[1,0] = exp(1.5)", expr_eval_d(v), exp(1.5), 1e-12);
            mat_get(E, 2, 0, &v);
            check_d("exp(lower Jordan)[2,0] = exp(1.5)/2", expr_eval_d(v), 0.5 * exp(1.5), 1e-12);
            mat_get(E, 2, 0, &v);
            check_expr_expr_contains("exp(lower Jordan)[2,0] stays symbolic in x", v, "exp(x)");
            test_expr_set_val_d(x, 2.0);
            mat_get(E, 2, 0, &v);
            check_d("exp(lower Jordan)[2,0] tracks x", expr_eval_d(v), 0.5 * exp(2.0), 1e-12);
        }

        mat_free(T);
        mat_free(E);
        expr_free(x);
        expr_free(one);
    }

    {
        expr_t *x = test_expr_new_named_var_d(1.0, "x");
        expr_t *one = test_expr_new_const_d(1.0);
        expr_t *two = test_expr_new_const_d(2.0);
        expr_t *vals[9] = {x, one, EXPR_ZERO, one, two, one, EXPR_ZERO, one, x};
        matrix_t *A = mat_create_expr(3, 3, vals);
        matrix_t *Aqc = NULL;
        matrix_t *Eqc = NULL;
        matrix_t *Lqc = NULL;
        matrix_t *Sqc = NULL;
        matrix_t *Gqc = NULL;

        print_mdv("A", A);
        check_bool("mat_exp(expr dense 3x3) currently unsupported", mat_exp(A) == NULL);
        check_bool("mat_log(expr dense 3x3) currently unsupported", mat_log(A) == NULL);
        check_bool("mat_sin(expr dense 3x3) currently unsupported", mat_sin(A) == NULL);
        check_bool("mat_gamma(expr dense 3x3) currently unsupported", mat_gamma(A) == NULL);

        Aqc = test_mat_evaluate_complex(A);
        check_bool("test_mat_evaluate_complex(expr dense 3x3) not NULL", Aqc != NULL);

        if (Aqc) {
            Eqc = mat_exp(Aqc);
            Lqc = mat_log(Aqc);
            Sqc = mat_sin(Aqc);
            Gqc = mat_gamma(Aqc);
        }

        check_bool("manual qc exp(expr dense 3x3) not NULL", Eqc != NULL);
        check_bool("manual qc log(expr dense 3x3) not NULL", Lqc != NULL);
        check_bool("manual qc sin(expr dense 3x3) not NULL", Sqc != NULL);
        check_bool("manual qc gamma(expr dense 3x3) not NULL", Gqc != NULL);

        check_bool("manual qc exp(expr dense 3x3) -> MAT_TYPE_NUMBER",
                   Eqc != NULL && mat_typeof(Eqc) == MAT_TYPE_NUMBER);
        check_bool("manual qc log(expr dense 3x3) -> MAT_TYPE_NUMBER",
                   Lqc != NULL && mat_typeof(Lqc) == MAT_TYPE_NUMBER);
        check_bool("manual qc sin(expr dense 3x3) -> MAT_TYPE_NUMBER",
                   Sqc != NULL && mat_typeof(Sqc) == MAT_TYPE_NUMBER);
        check_bool("manual qc gamma(expr dense 3x3) -> MAT_TYPE_NUMBER",
                   Gqc != NULL && mat_typeof(Gqc) == MAT_TYPE_NUMBER);

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Lqc);
        mat_free(Sqc);
        mat_free(Gqc);
        mat_free(A);
        expr_free(x);
        expr_free(one);
        expr_free(two);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[0 x 0][x 0 x][0 x 0]]", &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;
        double r;

        check_bool("dense expr cubic-linear 3x3 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense expr cubic-linear 3x3 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense expr cubic-linear 3x3 exp not NULL", E != NULL);
        check_bool("dense expr cubic-linear 3x3 sin not NULL", S != NULL);

        r = sqrt(8.0);
        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense cubic-linear 3x3)[0,0]", expr_eval_d(v), 0.5 * (cosh(r) + 1.0), 1e-12);
            check_expr_expr_contains("exp(dense cubic-linear 3x3)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(dense cubic-linear 3x3)[0,1]", expr_eval_d(v), sinh(r) / r * 2.0, 1e-12);
            mat_get(E, 0, 2, &v);
            check_d("exp(dense cubic-linear 3x3)[0,2]", expr_eval_d(v), 0.5 * (cosh(r) - 1.0), 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(dense cubic-linear 3x3)[0,0]", expr_eval_d(v), 0.0, 1e-12);
            mat_get(S, 0, 1, &v);
            check_d("sin(dense cubic-linear 3x3)[0,1]", expr_eval_d(v), sin(r) / r * 2.0, 1e-12);
            check_expr_expr_contains("sin(dense cubic-linear 3x3)[0,1] stays symbolic", v, "sin");
            mat_get(S, 0, 2, &v);
            check_d("sin(dense cubic-linear 3x3)[0,2]", expr_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("dense expr cubic-linear 3x3 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
        }

        r = sqrt(18.0);
        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(dense cubic-linear 3x3)[0,1] tracks x", expr_eval_d(v), sinh(r) / r * 3.0, 1e-12);
        }
        if (S) {
            mat_get(S, 1, 0, &v);
            check_d("sin(dense cubic-linear 3x3)[1,0] tracks x", expr_eval_d(v), sin(r) / r * 3.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[0 x x][x 0 x][x x 0]]", &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;

        check_bool("dense expr quadratic 3x3 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense expr quadratic 3x3 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense expr quadratic 3x3 exp not NULL", E != NULL);
        check_bool("dense expr quadratic 3x3 sin not NULL", S != NULL);

        if (A)
            print_mdv("A (dense quadratic 3x3)", A);
        if (E)
            print_mdv("exp(A)", E);
        if (S)
            print_mdv("sin(A)", S);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense quadratic 3x3)[0,0]", expr_eval_d(v), (exp(4.0) + 2.0 * exp(-2.0)) / 3.0, 1e-12);
            check_expr_expr_contains("exp(dense quadratic 3x3)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(dense quadratic 3x3)[0,1]", expr_eval_d(v), (exp(4.0) - exp(-2.0)) / 3.0, 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(dense quadratic 3x3)[0,0]", expr_eval_d(v), (sin(4.0) - 2.0 * sin(2.0)) / 3.0, 1e-12);
            check_expr_expr_contains("sin(dense quadratic 3x3)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 1, &v);
            check_d("sin(dense quadratic 3x3)[0,1]", expr_eval_d(v), (sin(4.0) + sin(2.0)) / 3.0, 1e-12);
        }

        if (bindings) {
            check_bool("dense expr quadratic 3x3 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense quadratic 3x3)[0,0] tracks x", expr_eval_d(v), (exp(6.0) + 2.0 * exp(-3.0)) / 3.0,
                    1e-12);
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_d("sin(dense quadratic 3x3)[0,1] tracks x", expr_eval_d(v), (sin(6.0) + sin(3.0)) / 3.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[x 1 1 1 1]"
                                           "[1 x 1 1 1]"
                                           "[1 1 x 1 1]"
                                           "[1 1 1 x 1]"
                                           "[1 1 1 1 x]]",
                                           &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;

        check_bool("uniform dense expr 5x5 input not NULL", A != NULL);
        if (bindings) {
            check_bool("uniform dense expr 5x5 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("uniform dense expr 5x5 exp not NULL", E != NULL);
        check_bool("uniform dense expr 5x5 sin not NULL", S != NULL);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(uniform dense 5x5)[0,0]", expr_eval_d(v), (4.0 * exp(1.0) + exp(6.0)) / 5.0, 1e-12);
            check_expr_expr_contains("exp(uniform dense 5x5)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(uniform dense 5x5)[0,1]", expr_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
            mat_get(E, 3, 4, &v);
            check_d("exp(uniform dense 5x5)[3,4]", expr_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(uniform dense 5x5)[0,0]", expr_eval_d(v), (4.0 * sin(1.0) + sin(6.0)) / 5.0, 1e-12);
            check_expr_expr_contains("sin(uniform dense 5x5)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 2, &v);
            check_d("sin(uniform dense 5x5)[0,2]", expr_eval_d(v), (sin(6.0) - sin(1.0)) / 5.0, 1e-12);
        }

        if (bindings) {
            check_bool("uniform dense expr 5x5 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(uniform dense 5x5)[0,1] tracks x", expr_eval_d(v), (exp(7.0) - exp(2.0)) / 5.0, 1e-12);
        }
        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(uniform dense 5x5)[0,0] tracks x", expr_eval_d(v), (4.0 * sin(2.0) + sin(7.0)) / 5.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("(7, x, 2, 1;"
                                           " 10, 2*x + 2, 4, 2;"
                                           " 15, 3*x, 8, 3;"
                                           " 20, 4*x, 8, 6)",
                                           &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;
        double cexp;
        double csin;

        check_bool("rank-one perturbation dense expr 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("rank-one perturbation dense expr 4x4 set x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("rank-one perturbation dense expr 4x4 exp not NULL", E != NULL);
        check_bool("rank-one perturbation dense expr 4x4 sin not NULL", S != NULL);

        cexp = (exp(23.0) - exp(2.0)) / 21.0;
        csin = (sin(23.0) - sin(2.0)) / 21.0;

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(rank-one perturbation 4x4)[0,0]", expr_eval_d(v), exp(2.0) + 5.0 * cexp, 1e-5);
            check_expr_expr_contains("exp(rank-one perturbation 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(rank-one perturbation 4x4)[0,1]", expr_eval_d(v), 3.0 * cexp, 1e-12);
            mat_get(E, 3, 2, &v);
            check_d("exp(rank-one perturbation 4x4)[3,2]", expr_eval_d(v), 8.0 * cexp, 1e-5);
        }

        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(rank-one perturbation 4x4)[1,1]", expr_eval_d(v), sin(2.0) + 6.0 * csin, 1e-12);
            check_expr_expr_contains("sin(rank-one perturbation 4x4)[1,1] stays symbolic", v, "sin");
            mat_get(S, 2, 0, &v);
            check_d("sin(rank-one perturbation 4x4)[2,0]", expr_eval_d(v), 15.0 * csin, 1e-12);
            mat_get(S, 0, 3, &v);
            check_d("sin(rank-one perturbation 4x4)[0,3]", expr_eval_d(v), csin, 1e-12);
        }

        if (bindings) {
            check_bool("rank-one perturbation dense expr 4x4 update x",
                       test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
        }

        cexp = (exp(25.0) - exp(2.0)) / 23.0;
        csin = (sin(25.0) - sin(2.0)) / 23.0;

        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(rank-one perturbation 4x4)[0,1] tracks x", expr_eval_d(v), 4.0 * cexp, 1e-5);
        }
        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(rank-one perturbation 4x4)[1,1] tracks x", expr_eval_d(v), sin(2.0) + 8.0 * csin, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[0 x 0 0]"
                                           "[x 0 x 0]"
                                           "[0 x 0 x]"
                                           "[0 0 x 0]]",
                                           &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        matrix_t *Aqc = NULL;
        matrix_t *Eqc = NULL;
        matrix_t *Eqc_expected = NULL;
        matrix_t *Sqc = NULL;
        matrix_t *Sqc_expected = NULL;
        expr_t *v = NULL;

        check_bool("dense expr biquadratic quartic 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense expr biquadratic quartic 4x4 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense expr biquadratic quartic 4x4 exp not NULL", E != NULL);
        check_bool("dense expr biquadratic quartic 4x4 sin not NULL", S != NULL);

        if (E && S) {
            Aqc = test_mat_evaluate_complex(A);
            Eqc = test_mat_evaluate_complex(E);
            Sqc = test_mat_evaluate_complex(S);
            Eqc_expected = Aqc ? mat_exp(Aqc) : NULL;
            Sqc_expected = Aqc ? mat_sin(Aqc) : NULL;

            check_bool("dense expr biquadratic quartic 4x4 evaluated exp not NULL", Eqc != NULL);
            check_bool("dense expr biquadratic quartic 4x4 evaluated sin not NULL", Sqc != NULL);
            check_bool("dense expr biquadratic quartic 4x4 numeric exp baseline not NULL", Eqc_expected != NULL);
            check_bool("dense expr biquadratic quartic 4x4 numeric sin baseline not NULL", Sqc_expected != NULL);
            if (Eqc && Eqc_expected) {
                bool ok = test_assert_matrix_complex_close(Eqc, Eqc_expected, 1e-12, __FILE__, __LINE__);
                if (!ok) {
                    mat_free(Aqc);
                    mat_free(Eqc);
                    mat_free(Eqc_expected);
                    mat_free(Sqc);
                    mat_free(Sqc_expected);
                    mat_free(A);
                    mat_free(E);
                    mat_free(S);
                    mat_bindings_free(bindings);
                    return;
                }
            }
            if (Sqc && Sqc_expected) {
                bool ok = test_assert_matrix_complex_close(Sqc, Sqc_expected, 1e-12, __FILE__, __LINE__);
                if (!ok) {
                    mat_free(Aqc);
                    mat_free(Eqc);
                    mat_free(Eqc_expected);
                    mat_free(Sqc);
                    mat_free(Sqc_expected);
                    mat_free(A);
                    mat_free(E);
                    mat_free(S);
                    mat_bindings_free(bindings);
                    return;
                }
            }
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_expr_expr_contains("exp(biquadratic quartic 4x4)[0,0] stays symbolic", v, "exp");
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_expr_expr_contains("sin(biquadratic quartic 4x4)[0,1] stays symbolic", v, "sin");
        }

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Eqc_expected);
        mat_free(Sqc);
        mat_free(Sqc_expected);
        Aqc = NULL;
        Eqc = NULL;
        Eqc_expected = NULL;
        Sqc = NULL;
        Sqc_expected = NULL;

        if (bindings) {
            check_bool("dense expr biquadratic quartic 4x4 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
        }

        if (E && S) {
            Aqc = test_mat_evaluate_complex(A);
            Eqc = test_mat_evaluate_complex(E);
            Sqc = test_mat_evaluate_complex(S);
            Eqc_expected = Aqc ? mat_exp(Aqc) : NULL;
            Sqc_expected = Aqc ? mat_sin(Aqc) : NULL;

            if (Eqc && Eqc_expected) {
                bool ok = test_assert_matrix_complex_close(Eqc, Eqc_expected, 1e-12, __FILE__, __LINE__);
                if (!ok) {
                    mat_free(Aqc);
                    mat_free(Eqc);
                    mat_free(Eqc_expected);
                    mat_free(Sqc);
                    mat_free(Sqc_expected);
                    mat_free(A);
                    mat_free(E);
                    mat_free(S);
                    mat_bindings_free(bindings);
                    return;
                }
            }
            if (Sqc && Sqc_expected) {
                bool ok = test_assert_matrix_complex_close(Sqc, Sqc_expected, 1e-12, __FILE__, __LINE__);
                if (!ok) {
                    mat_free(Aqc);
                    mat_free(Eqc);
                    mat_free(Eqc_expected);
                    mat_free(Sqc);
                    mat_free(Sqc_expected);
                    mat_free(A);
                    mat_free(E);
                    mat_free(S);
                    mat_bindings_free(bindings);
                    return;
                }
            }
        }

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Eqc_expected);
        mat_free(Sqc);
        mat_free(Sqc_expected);
        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[x 1 0 0][1 x 0 0][0 0 y 1][0 0 1 y]]", &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;

        check_bool("block-diagonal dense expr 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("block-diagonal dense expr 4x4 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
            check_bool("block-diagonal dense expr 4x4 set y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("block-diagonal dense expr 4x4 exp not NULL", E != NULL);
        check_bool("block-diagonal dense expr 4x4 sin not NULL", S != NULL);

        if (A)
            print_mdv("A (block-diagonal dense 4x4)", A);
        if (E)
            print_mdv("exp(A)", E);
        if (S)
            print_mdv("sin(A)", S);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(block-diagonal 4x4)[0,0]", expr_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
            check_expr_expr_contains("exp(block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(block-diagonal 4x4)[0,1]", expr_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
            mat_get(E, 0, 2, &v);
            check_d("exp(block-diagonal 4x4)[0,2] stays zero", expr_eval_d(v), 0.0, 1e-12);
            mat_get(E, 2, 2, &v);
            check_d("exp(block-diagonal 4x4)[2,2]", expr_eval_d(v), 0.5 * (exp(4.0) + exp(2.0)), 1e-12);
        }

        if (S) {
            mat_get(S, 2, 2, &v);
            check_d("sin(block-diagonal 4x4)[2,2]", expr_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
            check_expr_expr_contains("sin(block-diagonal 4x4)[2,2] stays symbolic", v, "sin");
            mat_get(S, 2, 3, &v);
            check_d("sin(block-diagonal 4x4)[2,3]", expr_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
            mat_get(S, 1, 3, &v);
            check_d("sin(block-diagonal 4x4)[1,3] stays zero", expr_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("block-diagonal dense expr 4x4 update x", test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
            check_bool("block-diagonal dense expr 4x4 update y", test_mat_bindings_set_d(bindings, "y", 5.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(block-diagonal 4x4)[0,0] tracks x", expr_eval_d(v), 0.5 * (exp(5.0) + exp(3.0)), 1e-12);
        }
        if (S) {
            mat_get(S, 2, 3, &v);
            check_d("sin(block-diagonal 4x4)[2,3] tracks y", expr_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)), 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }

    {
        mat_bindings_t *bindings = NULL;
        matrix_t *A = mat_from_string_expr("[[x 0 1 0][0 y 0 1][1 0 x 0][0 1 0 y]]", &bindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        expr_t *v = NULL;

        check_bool("permuted block-diagonal dense expr 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("permuted block-diagonal dense expr 4x4 set x",
                       test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
            check_bool("permuted block-diagonal dense expr 4x4 set y",
                       test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("permuted block-diagonal dense expr 4x4 exp not NULL", E != NULL);
        check_bool("permuted block-diagonal dense expr 4x4 sin not NULL", S != NULL);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,0]", expr_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
            check_expr_expr_contains("exp(permuted block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 2, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,2]", expr_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
            mat_get(E, 0, 1, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,1] stays zero", expr_eval_d(v), 0.0, 1e-12);
            mat_get(E, 1, 3, &v);
            check_d("exp(permuted block-diagonal 4x4)[1,3]", expr_eval_d(v), 0.5 * (exp(4.0) - exp(2.0)), 1e-12);
        }

        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,1]", expr_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
            check_expr_expr_contains("sin(permuted block-diagonal 4x4)[1,1] stays symbolic", v, "sin");
            mat_get(S, 1, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,3]", expr_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
            mat_get(S, 2, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[2,3] stays zero", expr_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("permuted block-diagonal dense expr 4x4 update x",
                       test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
            check_bool("permuted block-diagonal dense expr 4x4 update y",
                       test_mat_bindings_set_d(bindings, "y", 5.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 2, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,2] tracks x", expr_eval_d(v), 0.5 * (exp(5.0) - exp(3.0)),
                    1e-12);
        }
        if (S) {
            mat_get(S, 1, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,3] tracks y", expr_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)),
                    1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        mat_bindings_free(bindings);
    }
}

/* ------------------------------------------------------------------ 3×3 matrix function tests */

/*
 * Symmetric positive-definite 3×3 input exercises the full Schur-roundtrip
 * path on a genuinely dense matrix, rather than the simpler diagonal,
 * nilpotent, or 2×2 cases covered elsewhere in this file.
 */

void run_matrix_function_tests(void)
{
    TEST_RUN_CASE(test_mat_neg_convenience, NULL);
    TEST_RUN_CASE(test_number_function_matrix_parity, NULL);
    TEST_RUN_CASE(test_mat_harmonic_poly, NULL);
    TEST_RUN_CASE(test_eigen_d, NULL);
    TEST_RUN_CASE(test_eigen_mp_real, NULL);
    TEST_RUN_CASE(test_eigen_complex, NULL);
    TEST_RUN_CASE(test_eigen_num_hermitian, NULL);
    TEST_RUN_CASE(test_eigen_num_hermitian_high_precision, NULL);
    TEST_RUN_CASE(test_eigen_expr, NULL);
    TEST_RUN_CASE(test_eigenspace_expr, NULL);
    TEST_RUN_CASE(test_generalized_eigenspace_expr, NULL);
    TEST_RUN_CASE(test_jordan_chain_expr, NULL);
    TEST_RUN_CASE(test_jordan_profile_expr, NULL);
    TEST_RUN_CASE(test_mat_exp_d, NULL);
    TEST_RUN_CASE(test_mat_exp_mp_real, NULL);
    TEST_RUN_CASE(test_mat_exp_complex, NULL);
    TEST_RUN_CASE(test_mat_exp_singular, NULL);
    TEST_RUN_CASE(test_matrix_function_structure_preservation, NULL);
    TEST_RUN_CASE(test_mat_fun_singular_entire_d, NULL);
    TEST_RUN_CASE(test_mat_exp_null_safety, NULL);
    TEST_RUN_CASE(test_mat_sin_d, NULL);
    TEST_RUN_CASE(test_mat_sin_mp_real, NULL);
    TEST_RUN_CASE(test_mat_sin_complex, NULL);
    TEST_RUN_CASE(test_mat_sin_null_safety, NULL);
    TEST_RUN_CASE(test_mat_cos_d, NULL);
    TEST_RUN_CASE(test_mat_cos_mp_real, NULL);
    TEST_RUN_CASE(test_mat_cos_complex, NULL);
    TEST_RUN_CASE(test_mat_tan_d, NULL);
    TEST_RUN_CASE(test_mat_sinh_d, NULL);
    TEST_RUN_CASE(test_mat_cosh_d, NULL);
    TEST_RUN_CASE(test_mat_tanh_d, NULL);
    TEST_RUN_CASE(test_mat_trig_null_safety, NULL);
    TEST_RUN_CASE(test_mat_sqrt_d, NULL);
    TEST_RUN_CASE(test_mat_sqrt_mp_real, NULL);
    TEST_RUN_CASE(test_mat_log_d, NULL);
    TEST_RUN_CASE(test_mat_asin_d, NULL);
    TEST_RUN_CASE(test_mat_acos_d, NULL);
    TEST_RUN_CASE(test_mat_atan_d, NULL);
    TEST_RUN_CASE(test_mat_asinh_d, NULL);
    TEST_RUN_CASE(test_mat_acosh_d, NULL);
    TEST_RUN_CASE(test_mat_atanh_d, NULL);
    TEST_RUN_CASE(test_mat_inv_trig_null_safety, NULL);
    TEST_RUN_CASE(test_eigen_general_d, NULL);
    TEST_RUN_CASE(test_eigen_general_mp_real, NULL);
    TEST_RUN_CASE(test_eigen_general_num_high_precision, NULL);
    TEST_RUN_CASE(test_mat_nilpotent_d, NULL);
    TEST_RUN_CASE(test_mat_algebraic_ids_d, NULL);
    TEST_RUN_CASE(test_mat_roundtrips_d, NULL);
    TEST_RUN_CASE(test_mat_pow_int_d, NULL);
    TEST_RUN_CASE(test_mat_pow_num, NULL);
    TEST_RUN_CASE(test_mat_erf_d, NULL);
    TEST_RUN_CASE(test_mat_erfc_d, NULL);
    TEST_RUN_CASE(test_mat_special_unary_extensions, NULL);
    TEST_RUN_CASE(test_mat_special_unary_square_extensions, NULL);
    TEST_RUN_CASE(test_mat_typeof, NULL);
    TEST_RUN_CASE(test_number_matrix_functions, NULL);
    TEST_RUN_CASE(test_expr_matrix_functions, NULL);
    TEST_RUN_CASE(test_expr_matrix_functions_extended, NULL);
    TEST_RUN_CASE(test_mat_simplify_symbolic_helper, NULL);
}
