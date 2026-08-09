#include "test_matrix.h"

typedef struct {
    char *label;
    char *tex;
} matrix_tex_preview_entry_t;

static matrix_tex_preview_entry_t *g_matrix_tex_preview_entries = NULL;
static size_t g_matrix_tex_preview_count = 0u;
static size_t g_matrix_tex_preview_cap = 0u;

static char *matrix_tex_preview_strdup(const char *s)
{
    size_t n;
    char *copy;

    if (!s)
        return NULL;

    n = strlen(s) + 1u;
    copy = malloc(n);
    if (!copy)
        return NULL;
    memcpy(copy, s, n);
    return copy;
}

static char *matrix_tex_preview_path_from_source(const char *source_file)
{
    size_t len = strlen(source_file);
    char *path = malloc(len + 5u);

    if (!path)
        return NULL;

    memcpy(path, source_file, len + 1u);
    if (len >= 2u && strcmp(path + len - 2u, ".c") == 0)
        strcpy(path + len - 2u, ".tex");
    else
        strcat(path, ".tex");

    return path;
}

static void matrix_tex_preview_write_escaped(FILE *f, const char *s)
{
    const char *p;

    if (!s)
        return;

    for (p = s; *p; ++p) {
        switch (*p) {
            case '\\':
                fputs("\\textbackslash{}", f);
                break;
            case '{':
                fputs("\\{", f);
                break;
            case '}':
                fputs("\\}", f);
                break;
            case '_':
                fputs("\\_", f);
                break;
            case '^':
                fputs("\\^{}", f);
                break;
            case '%':
                fputs("\\%", f);
                break;
            case '&':
                fputs("\\&", f);
                break;
            case '#':
                fputs("\\#", f);
                break;
            case '$':
                fputs("\\$", f);
                break;
            default:
                fputc(*p, f);
                break;
        }
    }
}

static void matrix_tex_preview_emit_case(const char *source_file, const char *label, const char *tex)
{
    char *path;
    FILE *f;
    size_t i;

    if (!label || !tex)
        return;

    if (g_matrix_tex_preview_count == g_matrix_tex_preview_cap) {
        size_t new_cap = g_matrix_tex_preview_cap == 0u ? 8u : g_matrix_tex_preview_cap * 2u;
        matrix_tex_preview_entry_t *new_entries = realloc(g_matrix_tex_preview_entries, new_cap * sizeof(*new_entries));
        if (!new_entries)
            return;
        g_matrix_tex_preview_entries = new_entries;
        g_matrix_tex_preview_cap = new_cap;
    }

    g_matrix_tex_preview_entries[g_matrix_tex_preview_count].label = matrix_tex_preview_strdup(label);
    g_matrix_tex_preview_entries[g_matrix_tex_preview_count].tex = matrix_tex_preview_strdup(tex);
    if (!g_matrix_tex_preview_entries[g_matrix_tex_preview_count].label ||
        !g_matrix_tex_preview_entries[g_matrix_tex_preview_count].tex) {
        free(g_matrix_tex_preview_entries[g_matrix_tex_preview_count].label);
        free(g_matrix_tex_preview_entries[g_matrix_tex_preview_count].tex);
        g_matrix_tex_preview_entries[g_matrix_tex_preview_count].label = NULL;
        g_matrix_tex_preview_entries[g_matrix_tex_preview_count].tex = NULL;
        return;
    }
    ++g_matrix_tex_preview_count;

    path = matrix_tex_preview_path_from_source(source_file);
    if (!path)
        return;

    f = fopen(path, "wb");
    if (!f) {
        free(path);
        return;
    }

    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\usepackage[margin=1in]{geometry}\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\section*{Generated TeX Samples}\n");
    fprintf(f, "\\noindent Source: \\texttt{");
    matrix_tex_preview_write_escaped(f, source_file);
    fprintf(f, "}\n\n");

    for (i = 0u; i < g_matrix_tex_preview_count; ++i) {
        fprintf(f, "\\subsection*{Sample %zu}\n", i + 1u);
        fprintf(f, "\\noindent\\texttt{");
        matrix_tex_preview_write_escaped(f, g_matrix_tex_preview_entries[i].label);
        fprintf(f,
                "}\n"
                "\\begin{flushleft}\n"
                "$\\displaystyle %s$\n"
                "\\end{flushleft}\n\n",
                g_matrix_tex_preview_entries[i].tex);
    }

    fprintf(f, "\\end{document}\n");
    fclose(f);
    free(path);
}

static void matrix_tex_preview_cleanup(void)
{
    for (size_t i = 0u; i < g_matrix_tex_preview_count; ++i) {
        free(g_matrix_tex_preview_entries[i].label);
        free(g_matrix_tex_preview_entries[i].tex);
    }
    free(g_matrix_tex_preview_entries);
    g_matrix_tex_preview_entries = NULL;
    g_matrix_tex_preview_count = 0u;
    g_matrix_tex_preview_cap = 0u;
}

static void check_matrix_tostring_expr_double(const char *label, const expr_t *dv, double expected, double tol)
{
    number_t got = expr_eval(dv);
    number_t want = num_create_from_double(expected);
    number_t diff = num_sub(got, want);
    number_t mag = num_abs(diff);
    double err = num_to_double(mag);

    check_bool(label, err <= tol);

    num_destroy(&mag);
    num_destroy(&diff);
    num_destroy(&want);
    num_destroy(&got);
}

static char *matrix_tostring_num_to_cstr(const number_t value)
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

static char *format_matrix_test_num_at_own_precision(const number_t value, int scientific)
{
    char fmt[32];
    char *out;
    size_t significant_digits = num_get_prec_digits(value);
    int needed;
    size_t precision;

    if (num_is_exact(value) || significant_digits == 0u)
        return matrix_tostring_num_to_cstr(value);

    precision = significant_digits > 0u ? significant_digits - 1u : 0u;
    snprintf(fmt, sizeof(fmt), "%%.%zu%c", precision, scientific ? 'N' : 'n');
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

static void test_mat_to_string_numeric(void)
{
    number_t vals[4] = {num_create_from_long(1), num_create_from_long(2), num_create_from_long(3),
                        num_create_from_long(4)};
    matrix_t *A = mat_create(2, 2, vals);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_scientific = mat_to_string(A, MAT_STRING_LAYOUT_SCIENTIFIC);

    check_bool("mat_to_string number inline string non-null", inline_pretty != NULL);
    check_bool("mat_to_string number inline has matrix delimiters",
               inline_pretty && inline_pretty[0] == '(' && strchr(inline_pretty, ';') != NULL);
    check_bool("mat_to_string number inline keeps real entries",
               inline_pretty && strstr(inline_pretty, "1") != NULL && strstr(inline_pretty, "2") != NULL &&
                   strstr(inline_pretty, "3") != NULL && strstr(inline_pretty, "4") != NULL);
    check_bool("mat_to_string number layout scientific non-null", layout_scientific != NULL);
    check_bool("mat_to_string number layout scientific has newline",
               layout_scientific && strchr(layout_scientific, '\n') != NULL);
    check_bool("mat_to_string number layout scientific keeps numeric entries",
               layout_scientific && strstr(layout_scientific, "1") != NULL && strstr(layout_scientific, "2") != NULL &&
                   strstr(layout_scientific, "3") != NULL && strstr(layout_scientific, "4") != NULL);

    free(inline_pretty);
    free(layout_scientific);
    mat_free(A);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_to_string_numeric_tex(void)
{
    number_t vals[4] = {num_create_from_long(1), num_create_from_long(2), num_create_from_long(3),
                        num_create_from_long(4)};
    matrix_t *A = mat_create(2, 2, vals);
    char *tex = mat_to_string(A, MAT_STRING_TEX);

    matrix_tex_preview_emit_case(__FILE__, "numeric matrix (TEX)", tex);

    check_bool("mat_to_string number tex non-null", tex != NULL);
    check_bool("mat_to_string number tex begins bmatrix", tex && strstr(tex, "\\begin{bmatrix}") != NULL);
    check_bool("mat_to_string number tex uses column separator", tex && strstr(tex, "1 & 2") != NULL);
    check_bool("mat_to_string number tex uses row separator", tex && strstr(tex, " \\\\ 3 & 4") != NULL);

    free(tex);
    mat_free(A);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_to_string_number_precision(void)
{
    number_t vals[4];
    matrix_t *A;
    char *inline_pretty;
    char *layout_scientific;
    char *expected_pretty;
    char *expected_scientific;

    vals[0] = num_create_from_string("1.25");
    vals[1] = num_create_from_string("1 + 2i");
    check_bool("mat_to_string number real precision set", num_set_prec_bits(&vals[0], 512u) == 0);
    check_bool("mat_to_string number complex precision set", num_set_prec_bits(&vals[1], 384u) == 0);
    vals[2] = num_create_from_string("1/2");
    vals[3] = num_create_from_long(3);
    A = mat_create(2, 2, vals);
    inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    layout_scientific = mat_to_string(A, MAT_STRING_LAYOUT_SCIENTIFIC);
    expected_pretty = format_matrix_test_num_at_own_precision(vals[0], 0);
    expected_scientific = format_matrix_test_num_at_own_precision(vals[1], 1);

    check_bool("mat_to_string number inline string non-null", inline_pretty != NULL);
    check_bool("mat_to_string number layout string non-null", layout_scientific != NULL);
    check_bool("mat_to_string number inline keeps full precision text",
               inline_pretty && expected_pretty && strstr(inline_pretty, expected_pretty) != NULL);
    check_bool("mat_to_string number layout keeps scientific precision text",
               layout_scientific && expected_scientific && strstr(layout_scientific, expected_scientific) != NULL);
    check_bool("mat_to_string number inline preserves rational syntax",
               inline_pretty && strstr(inline_pretty, "½") != NULL);
    check_bool("mat_to_string number layout preserves rational syntax",
               layout_scientific && strstr(layout_scientific, "½") != NULL);
    check_bool("mat_to_string number inline preserves integer syntax",
               inline_pretty && strstr(inline_pretty, "3") != NULL);
    check_bool("mat_to_string number layout preserves integer syntax",
               layout_scientific && strstr(layout_scientific, "3") != NULL);

    printf("    number matrix pretty [0,0]\n");
    printf("        expected = %s\n", expected_pretty ? expected_pretty : "(unavailable)");
    printf("        got      = %s\n", inline_pretty ? inline_pretty : "(unavailable)");
    printf("        error    = %s\n",
           inline_pretty && expected_pretty && strstr(inline_pretty, expected_pretty) ? "0.000000E+0" : "(mismatch)");
    printf("        precision: %zu bits, %zu significant digits\n", num_get_prec_bits(vals[0]),
           num_get_prec_digits(vals[0]));

    printf("    number matrix scientific [0,1]\n");
    printf("        expected = %s\n", expected_scientific ? expected_scientific : "(unavailable)");
    printf("        got      = %s\n", layout_scientific ? layout_scientific : "(unavailable)");
    printf("        error    = %s\n",
           layout_scientific && expected_scientific && strstr(layout_scientific, expected_scientific) ? "0.000000E+0"
                                                                                                      : "(mismatch)");
    printf("        precision: %zu bits, %zu significant digits\n", num_get_prec_bits(vals[1]),
           num_get_prec_digits(vals[1]));

    free(expected_scientific);
    free(expected_pretty);
    free(layout_scientific);
    free(inline_pretty);
    mat_free(A);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
}

static void test_mat_to_string_symbolic(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("{ (x, 1; 1, c1) | x = 2; c1 = 3 }", &bindings);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_pretty = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);

    check_bool("mat_to_string symbolic inline non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic layout non-null", layout_pretty != NULL);
    check_bool("mat_to_string symbolic inline wrapped", inline_pretty && strstr(inline_pretty, "{ (") != NULL);
    check_bool("mat_to_string symbolic inline has bindings", inline_pretty && strstr(inline_pretty, "x = 2") != NULL);
    check_bool("mat_to_string symbolic layout has newline", layout_pretty && strchr(layout_pretty, '\n') != NULL);

    free(inline_pretty);
    free(layout_pretty);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_tex(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("{ (x0, 1; 1, c1) | x0 = 2; c1 = 3 }", &bindings);
    char *tex = mat_to_string(A, MAT_STRING_TEX);

    matrix_tex_preview_emit_case(__FILE__, "symbolic matrix with bindings (TEX)", tex);

    check_bool("mat_to_string symbolic tex non-null", tex != NULL);
    check_bool("mat_to_string symbolic tex wrapped", tex && strstr(tex, "\\left\\{") != NULL);
    check_bool("mat_to_string symbolic tex has bmatrix", tex && strstr(tex, "\\begin{bmatrix}") != NULL);
    check_bool("mat_to_string symbolic tex has subscripted names",
               tex && strstr(tex, "x_{0}") != NULL && strstr(tex, "c_{1}") != NULL);
    check_bool("mat_to_string symbolic tex has middle bar", tex && strstr(tex, "\\middle|") != NULL);

    free(tex);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_tex_exact(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("{ (sin(x0), exp(c1); ln(x0), c1^2) | x0 = 2; c1 = 5 }", &bindings);
    char *tex = mat_to_string(A, MAT_STRING_TEX);

    const char *expect = "\\left\\{ \\begin{bmatrix}\\sin(x_{0}) & e^{c_{1}} \\\\ "
                         "\\ln(x_{0}) & c_{1}^{2}\\end{bmatrix} \\;\\middle|\\; "
                         "x_{0} = 2; c_{1} = 5 \\right\\}";

    matrix_tex_preview_emit_case(__FILE__, "symbolic matrix exact with bindings (TEX)", tex);

    check_bool("mat_to_string symbolic tex exact non-null", tex != NULL);
    check_bool("mat_to_string symbolic tex exact string", tex && strcmp(tex, expect) == 0);

    free(tex);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_tex_no_bindings_exact(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(sin(x0), exp(c1); ln(x0), c1^2)", &bindings);
    char *tex = mat_to_string(A, MAT_STRING_TEX);
    const char *expect = "\\begin{bmatrix}\\sin(x_{0}) & e^{c_{1}} \\\\ "
                         "\\ln(x_{0}) & c_{1}^{2}\\end{bmatrix}";

    matrix_tex_preview_emit_case(__FILE__, "symbolic matrix exact without bindings (TEX)", tex);

    check_bool("mat_to_string symbolic tex no-bindings non-null", tex != NULL);
    check_bool("mat_to_string symbolic tex no-bindings omits wrapper",
               tex && strstr(tex, "\\left\\{") == NULL && strstr(tex, "\\middle|") == NULL);
    check_bool("mat_to_string symbolic tex no-bindings exact string", tex && strcmp(tex, expect) == 0);

    free(tex);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_all_nan_elides_wrapper(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, c1)", &bindings);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_pretty = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);

    check_bool("mat_to_string symbolic all-NaN inline string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic all-NaN layout string non-null", layout_pretty != NULL);
    check_bool("mat_to_string symbolic all-NaN inline omits wrapper",
               inline_pretty && strcmp(inline_pretty, "(x, c₁)") == 0);
    check_bool("mat_to_string symbolic all-NaN layout omits wrapper",
               layout_pretty && strcmp(layout_pretty, "(\n  x c₁\n)") == 0);

    free(inline_pretty);
    free(layout_pretty);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_all_nan_tex_elides_wrapper(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, c1)", &bindings);
    char *tex = mat_to_string(A, MAT_STRING_TEX);

    matrix_tex_preview_emit_case(__FILE__, "symbolic all-NaN matrix without bindings (TEX)", tex);

    check_bool("mat_to_string symbolic all-NaN tex non-null", tex != NULL);
    check_bool("mat_to_string symbolic all-NaN tex omits wrapper",
               tex && strstr(tex, "\\left\\{") == NULL && strstr(tex, "\\middle|") == NULL);
    check_bool("mat_to_string symbolic all-NaN tex exact string",
               tex && strcmp(tex, "\\begin{bmatrix}x & c_{1}\\end{bmatrix}") == 0);

    free(tex);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_roundtrip(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, c1; x*y, [radius])", &bindings);
    char *inline_pretty = NULL;
    mat_bindings_t *roundtrip_bindings = NULL;
    matrix_t *roundtrip = NULL;
    expr_t *dv = NULL;

    check_bool("mat_to_string symbolic roundtrip source non-null", A != NULL);
    check_bool("mat_to_string symbolic roundtrip source bindings returned", bindings != NULL);
    check_bool("mat_to_string symbolic roundtrip set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set c₁", test_mat_bindings_set_d(bindings, "c₁", 5.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set [radius]",
               test_mat_bindings_set_d(bindings, "[radius]", 7.0) == 0);

    inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    check_bool("mat_to_string symbolic roundtrip string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic roundtrip keeps wrapper", inline_pretty && strstr(inline_pretty, "{ (") != NULL);

    roundtrip = mat_from_string_expr(inline_pretty, &roundtrip_bindings);
    check_bool("mat_to_string symbolic roundtrip reparses", roundtrip != NULL);
    check_bool("mat_to_string symbolic roundtrip reparsed type", roundtrip && mat_typeof(roundtrip) == MAT_TYPE_EXPR);
    check_bool("mat_to_string symbolic roundtrip x binding present", mat_bindings_get(roundtrip_bindings, "x") != NULL);
    check_bool("mat_to_string symbolic roundtrip c₁ binding present",
               mat_bindings_get(roundtrip_bindings, "c₁") != NULL);
    check_bool("mat_to_string symbolic roundtrip [radius] binding present",
               mat_bindings_get(roundtrip_bindings, "[radius]") != NULL);

    if (roundtrip) {
        mat_get(roundtrip, 0, 0, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic roundtrip x entry", dv, 2.0, 1e-18);
        mat_get(roundtrip, 0, 1, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic roundtrip c₁ entry", dv, 5.0, 1e-18);
        mat_get(roundtrip, 1, 0, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic roundtrip x*y entry", dv, 6.0, 1e-18);
        mat_get(roundtrip, 1, 1, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic roundtrip [radius] entry", dv, 7.0, 1e-18);
    }

    free(inline_pretty);
    mat_bindings_free(roundtrip_bindings);
    mat_free(roundtrip);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_derivative_roundtrip(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(x, c1; x*y, y)", &bindings);
    expr_t *x_binding = NULL;
    matrix_t *Dx = NULL;
    char *inline_pretty = NULL;
    mat_bindings_t *roundtrip_bindings = NULL;
    matrix_t *roundtrip = NULL;
    expr_t *dv = NULL;

    check_bool("mat_to_string symbolic derivative source non-null", A != NULL);
    x_binding = mat_bindings_get(bindings, "x");
    check_bool("mat_to_string symbolic derivative x binding present", x_binding != NULL);
    check_bool("mat_to_string symbolic derivative set y", test_mat_bindings_set_d(bindings, "y", 4.0) == 0);
    check_bool("mat_to_string symbolic derivative set c₁", test_mat_bindings_set_d(bindings, "c₁", 7.0) == 0);

    if (A && x_binding)
        Dx = mat_deriv(A, x_binding);
    check_bool("mat_to_string symbolic derivative matrix non-null", Dx != NULL);

    inline_pretty = mat_to_string(Dx, MAT_STRING_INLINE_PRETTY);
    check_bool("mat_to_string symbolic derivative string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic derivative keeps wrapper for concrete remaining bindings",
               inline_pretty && strstr(inline_pretty, "{ (") != NULL);
    check_bool("mat_to_string symbolic derivative keeps y binding value",
               inline_pretty && strstr(inline_pretty, "y = 4") != NULL);

    roundtrip = mat_from_string_expr(inline_pretty, &roundtrip_bindings);
    check_bool("mat_to_string symbolic derivative reparses", roundtrip != NULL);
    check_bool("mat_to_string symbolic derivative reparsed type", roundtrip && mat_typeof(roundtrip) == MAT_TYPE_EXPR);
    check_bool("mat_to_string symbolic derivative reparsed has y", mat_bindings_get(roundtrip_bindings, "y") != NULL);
    check_bool("mat_to_string symbolic derivative reparsed omits x", mat_bindings_get(roundtrip_bindings, "x") == NULL);
    check_bool("mat_to_string symbolic derivative reparsed set y",
               test_mat_bindings_set_d(roundtrip_bindings, "y", 4.0) == 0);

    if (roundtrip) {
        mat_get(roundtrip, 0, 0, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic derivative reparsed [0,0]", dv, 1.0, 1e-18);
        mat_get(roundtrip, 0, 1, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic derivative reparsed [0,1]", dv, 0.0, 1e-18);
        mat_get(roundtrip, 1, 0, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic derivative reparsed [1,0]", dv, 4.0, 1e-18);
        mat_get(roundtrip, 1, 1, &dv);
        check_matrix_tostring_expr_double("mat_to_string symbolic derivative reparsed [1,1]", dv, 0.0, 1e-18);
    }

    free(inline_pretty);
    mat_bindings_free(roundtrip_bindings);
    mat_free(roundtrip);
    mat_free(Dx);
    mat_bindings_free(bindings);
    mat_free(A);
}

void run_matrix_tostring_tests(void)
{
    TEST_RUN_CASE(test_mat_to_string_numeric, NULL);
    TEST_RUN_CASE(test_mat_to_string_numeric_tex, NULL);
    TEST_RUN_CASE(test_mat_to_string_number_precision, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_tex, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_tex_exact, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_tex_no_bindings_exact, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_all_nan_elides_wrapper, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_all_nan_tex_elides_wrapper, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_roundtrip, NULL);
    TEST_RUN_CASE(test_mat_to_string_symbolic_derivative_roundtrip, NULL);
    matrix_tex_preview_cleanup();
}
