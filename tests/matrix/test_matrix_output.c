#include "test_matrix.h"

static void test_mat_sprintf_formats(void)
{
    qcomplex_t vals[4] = {
        qc_make(qf_from_double(1.0), qf_from_double(2.0)),
        qc_make(qf_from_double(3.0), QF_ZERO),
        qc_make(qf_from_double(4.0), QF_ZERO),
        qc_make(qf_from_double(5.0), qf_from_double(-6.0))
    };
    matrix_t *A = mat_create_qc(2, 2, vals);
    char buf[4096];
    int n_inline = mat_sprintf(buf, sizeof(buf), "%m", A);

    check_bool("mat_sprintf %m returns non-negative", n_inline >= 0);
    check_bool("mat_sprintf %m inline brackets",
               strstr(buf, "[[") != NULL && strstr(buf, "]]") != NULL);

    memset(buf, 0, sizeof(buf));
    check_bool("mat_sprintf %ML returns non-negative",
               mat_sprintf(buf, sizeof(buf), "%ML", A) >= 0);
    check_bool("mat_sprintf %ML has newline", strchr(buf, '\n') != NULL);
    check_bool("mat_sprintf %ML has exponent", strchr(buf, 'e') != NULL || strchr(buf, 'E') != NULL);

    mat_free(A);
}

static void test_mat_printf_smoke(void)
{
    qfloat_t vals[4] = {
        qf_from_double(1.0), qf_from_double(0.0),
        qf_from_double(0.0), qf_from_double(1.0)
    };
    matrix_t *A = mat_create_qf(2, 2, vals);
    int n = mat_printf("%m\n", A);

    check_bool("mat_printf returns positive count", n > 0);
    mat_free(A);
}

static void test_mat_sprintf_pretty_qcomplex(void)
{
    qcomplex_t vals[4] = {
        QC_ZERO,
        qc_make(QF_ZERO, qf_from_double(-1.0)),
        qc_make(QF_ZERO, qf_from_double(1.0)),
        QC_ZERO
    };
    matrix_t *A = mat_create_qc(2, 2, vals);
    char buf[4096];

    check_bool("mat_sprintf Pauli %ml returns non-negative",
               mat_sprintf(buf, sizeof(buf), "%ml", A) >= 0);
    check_bool("mat_sprintf Pauli pretty has -i",
               strstr(buf, "-i") != NULL);
    check_bool("mat_sprintf Pauli pretty has standalone i",
               strstr(buf, "\n  i") != NULL || strstr(buf, " i ") != NULL);
    check_bool("mat_sprintf Pauli pretty omits + 0i",
               strstr(buf, "+ 0i") == NULL && strstr(buf, "+0i") == NULL);

    mat_free(A);
}

void run_matrix_output_tests(void)
{
    test_mat_sprintf_formats();
    test_mat_printf_smoke();
    test_mat_sprintf_pretty_qcomplex();
}
