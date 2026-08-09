#include "test_matrix.h"

static void test_mat_sprintf_formats(void)
{
    number_t vals[4];
    matrix_t *A;
    char buf[4096];
    int n_inline;

    vals[0] = num_create_from_string("1+2i");
    vals[1] = num_create_from_long(3);
    vals[2] = num_create_from_long(4);
    vals[3] = num_create_from_string("5-6i");
    A = mat_create(2, 2, vals);
    n_inline = mat_sprintf(buf, sizeof(buf), "%m", A);

    check_bool("mat_sprintf %m returns non-negative", n_inline >= 0);
    check_bool("mat_sprintf %m inline delimiters", strstr(buf, "(") != NULL && strstr(buf, ";") != NULL);

    memset(buf, 0, sizeof(buf));
    check_bool("mat_sprintf %ML returns non-negative", mat_sprintf(buf, sizeof(buf), "%ML", A) >= 0);
    check_bool("mat_sprintf %ML has newline", strchr(buf, '\n') != NULL);
    check_bool("mat_sprintf %ML has long fixed-point formatting",
               strstr(buf, "1.000000000000") != NULL && strstr(buf, "5.000000000000") != NULL);

    mat_free(A);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_text_formatters(void)
{
    number_t vals[4];
    matrix_t *A;
    string_t *inline_text;
    string_t *layout_text;

    vals[0] = num_create_from_long(1);
    vals[1] = num_create_from_long(2);
    vals[2] = num_create_from_long(3);
    vals[3] = num_create_from_long(4);
    A = mat_create(2, 2, vals);

    inline_text = mat_to_text(A, MAT_STRING_INLINE_PRETTY);
    layout_text = mat_sprintf_text("A = %ml", A);

    check_bool("mat_to_text returns string_t", inline_text != NULL);
    check_bool("mat_to_text inline delimiters",
               inline_text && string_starts_with(inline_text, "(") && string_find(inline_text, ";") >= 0);
    check_bool("mat_sprintf_text returns string_t", layout_text != NULL);
    check_bool("mat_sprintf_text layout has prefix and newline",
               layout_text && string_starts_with(layout_text, "A = (") && string_find(layout_text, "\n") >= 0);

    string_free(inline_text);
    string_free(layout_text);
    mat_free(A);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_printf_smoke(void)
{
    number_t vals[4];
    matrix_t *A;
    const char *path = NULL;
    FILE *captured = NULL;
    char buf[256] = {0};
    int n;
    int saved_stdout;

    vals[0] = num_create_from_long(1);
    vals[1] = num_create_from_string("1/2");
    vals[2] = num_create_from_long(0);
    vals[3] = num_create_from_long(1);
    A = mat_create(2, 2, vals);
    saved_stdout = test_case_begin_stdout_capture("matrix-printf-smoke.txt", &path);
    check_bool("stdout capture for mat_printf available", saved_stdout >= 0);
    n = mat_printf("%m\n", A);
    check_bool("stdout capture for mat_printf closes cleanly", test_case_end_stdout_capture(saved_stdout));
    check_bool("mat_printf returns positive count", n > 0);
    captured = fopen(path, "r");
    check_bool("mat_printf capture file opens", captured != NULL);
    if (captured) {
        size_t used = fread(buf, 1u, sizeof(buf) - 1u, captured);
        buf[used] = '\0';
        fclose(captured);
    }
    check_bool("mat_printf writes inline matrix text", strstr(buf, "(") != NULL &&
                                                           (strstr(buf, "1/2") != NULL || strstr(buf, "½") != NULL) &&
                                                           strchr(buf, '\n') != NULL);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
    mat_free(A);
}

static void test_mat_sprintf_number_precision(void)
{
    number_t vals[4];
    matrix_t *A;
    char buf[4096];
    string_t *expected = NULL;

    vals[0] = num_create_from_string("1.25");
    check_bool("mat_sprintf number precision set", num_set_prec_bits(&vals[0], 512u) == 0);
    vals[1] = num_create_from_string("1/2");
    vals[2] = num_create_from_long(3);
    vals[3] = num_create_from_long(4);
    A = mat_create(2, 2, vals);
    expected = num_to_string(vals[1]);

    check_bool("mat_sprintf number returns non-negative", mat_sprintf(buf, sizeof(buf), "%m", A) >= 0);
    check_bool("mat_sprintf number preserves rational text", expected && strstr(buf, string_c_str(expected)) != NULL);

    string_free(expected);
    mat_free(A);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_sprintf_pretty_number_complex(void)
{
    number_t vals[4];
    matrix_t *A;
    char buf[4096];

    vals[0] = NUM_ZERO;
    vals[1] = num_create_from_string("-i");
    vals[2] = num_create_from_string("i");
    vals[3] = NUM_ZERO;
    A = mat_create(2, 2, vals);

    check_bool("mat_sprintf Pauli %ml returns non-negative", mat_sprintf(buf, sizeof(buf), "%ml", A) >= 0);
    check_bool("mat_sprintf Pauli pretty has negative imaginary entry",
               strstr(buf, " - 1.") != NULL || strstr(buf, "- 1i") != NULL || strstr(buf, "-1i") != NULL ||
                   strstr(buf, "-i") != NULL);
    check_bool("mat_sprintf Pauli pretty has positive imaginary entry",
               strstr(buf, " + 1.") != NULL || strstr(buf, "+ 1i") != NULL || strstr(buf, "+1i") != NULL ||
                   strstr(buf, "\n  i") != NULL || strstr(buf, " i ") != NULL);
    check_bool("mat_sprintf Pauli pretty omits + 0i", strstr(buf, "+ 0i") == NULL && strstr(buf, "+0i") == NULL);

    mat_free(A);
    num_destroy(&vals[1]);
    num_destroy(&vals[2]);
}

void run_matrix_output_tests(void)
{
    TEST_RUN_CASE(test_mat_sprintf_formats, NULL);
    TEST_RUN_CASE(test_mat_text_formatters, NULL);
    TEST_RUN_CASE(test_mat_printf_smoke, NULL);
    TEST_RUN_CASE(test_mat_sprintf_number_precision, NULL);
    TEST_RUN_CASE(test_mat_sprintf_pretty_number_complex, NULL);
}
