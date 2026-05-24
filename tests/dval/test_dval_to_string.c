#include "test_dval.h"

typedef struct {
    char *label;
    char *tex;
} tex_preview_entry_t;

static tex_preview_entry_t *g_tex_preview_entries = NULL;
static size_t g_tex_preview_count = 0u;
static size_t g_tex_preview_cap = 0u;
static int g_tex_preview_cleanup_registered = 0;

static void tex_preview_cleanup(void)
{
    size_t i;

    for (i = 0u; i < g_tex_preview_count; ++i) {
        free(g_tex_preview_entries[i].label);
        free(g_tex_preview_entries[i].tex);
    }
    free(g_tex_preview_entries);
    g_tex_preview_entries = NULL;
    g_tex_preview_count = 0u;
    g_tex_preview_cap = 0u;
}

static char *tex_preview_strdup(const char *s)
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

static char *tex_preview_path_from_source(const char *source_file)
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

static void tex_preview_write_escaped(FILE *f, const char *s)
{
    const char *p;

    if (!s)
        return;

    for (p = s; *p; ++p) {
        switch (*p) {
            case '\\': fputs("\\textbackslash{}", f); break;
            case '{':  fputs("\\{", f); break;
            case '}':  fputs("\\}", f); break;
            case '_':  fputs("\\_", f); break;
            case '^':  fputs("\\^{}", f); break;
            case '%':  fputs("\\%", f); break;
            case '&':  fputs("\\&", f); break;
            case '#':  fputs("\\#", f); break;
            case '$':  fputs("\\$", f); break;
            default:   fputc(*p, f); break;
        }
    }
}

static void tex_preview_emit_case(const char *source_file,
                                  const char *label,
                                  const char *tex)
{
    char *path;
    FILE *f;
    size_t i;

    if (!label || !tex)
        return;

    if (!g_tex_preview_cleanup_registered) {
        atexit(tex_preview_cleanup);
        g_tex_preview_cleanup_registered = 1;
    }

    if (g_tex_preview_count == g_tex_preview_cap) {
        size_t new_cap = g_tex_preview_cap == 0u ? 8u : g_tex_preview_cap * 2u;
        tex_preview_entry_t *new_entries =
            realloc(g_tex_preview_entries, new_cap * sizeof(*new_entries));
        if (!new_entries)
            return;
        g_tex_preview_entries = new_entries;
        g_tex_preview_cap = new_cap;
    }

    g_tex_preview_entries[g_tex_preview_count].label = tex_preview_strdup(label);
    g_tex_preview_entries[g_tex_preview_count].tex = tex_preview_strdup(tex);
    if (!g_tex_preview_entries[g_tex_preview_count].label ||
        !g_tex_preview_entries[g_tex_preview_count].tex) {
        free(g_tex_preview_entries[g_tex_preview_count].label);
        free(g_tex_preview_entries[g_tex_preview_count].tex);
        g_tex_preview_entries[g_tex_preview_count].label = NULL;
        g_tex_preview_entries[g_tex_preview_count].tex = NULL;
        return;
    }
    ++g_tex_preview_count;

    path = tex_preview_path_from_source(source_file);
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
    tex_preview_write_escaped(f, source_file);
    fprintf(f, "}\n\n");

    for (i = 0u; i < g_tex_preview_count; ++i) {
        fprintf(f, "\\subsection*{Sample %zu}\n", i + 1u);
        fprintf(f, "\\noindent\\texttt{");
        tex_preview_write_escaped(f, g_tex_preview_entries[i].label);
        fprintf(f, "}\n"
                   "\\begin{flushleft}\n"
                   "$\\displaystyle %s$\n"
                   "\\end{flushleft}\n\n",
                g_tex_preview_entries[i].tex);
    }

    fprintf(f, "\\end{document}\n");
    fclose(f);
    free(path);
}

/* ------------------------------------------------------------------------- */
/* dv_to_string Tests                                                        */
/* ------------------------------------------------------------------------- */

/* Detect whether a string is multiline */
static int is_multiline(const char *s)
{
    return s && strchr(s, '\n');
}

/* Print aligned multiline blocks */
static void print_multiline(const char *label, const char *s)
{
    /* Pad label to fixed width so got/expected align */
    int base_indent = fprintf(stderr, "  %-8s ", label);

    if (!s) {
        fprintf(stderr, "(null)\n");
        return;
    }

    const char *p = s;
    int first = 1;

    while (*p) {
        if (!first) {
            /* indent continuation lines to same column */
            for (int i = 0; i < base_indent; i++)
                fputc(' ', stderr);
        }

        const char *nl = strchr(p, '\n');
        if (nl) {
            fwrite(p, 1, nl - p + 1, stderr);
            p = nl + 1;
        } else {
            fprintf(stderr, "%s\n", p);
            break;
        }

        first = 0;
    }
}

/* PASS with optional separator */
void to_string_pass(const char *msg, const char *got, const char *expected)
{
    fprintf(stderr, C_BOLD C_GREEN "PASS " C_RESET "%s\n" C_RESET, msg);

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

void to_string_fail(const char *file, int line, int col, const char *msg, const char *got, const char *expected)
{
    fprintf(stderr, C_BOLD C_RED "FAIL" C_RESET " %s: " C_RED "%s:%d:%d\n" C_RESET, msg, file, line, col);
    TEST_FAIL();

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

/* Compare two strings ignoring trailing whitespace */
int str_eq(const char *a, const char *b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);

    while (la > 0 && (a[la-1] == '\n' || a[la-1] == '\r' || a[la-1] == ' '  || a[la-1] == '\t'))
        --la;

    while (lb > 0 && (b[lb-1] == '\n' || b[lb-1] == '\r' || b[lb-1] == ' '  || b[lb-1] == '\t'))
        --lb;

    return la == lb && memcmp(a, b, la) == 0;
}

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} test_sbuf_t;

static int test_sbuf_reserve(test_sbuf_t *b, size_t extra)
{
    size_t need = b->len + extra + 1u;
    char *next;

    if (need <= b->cap)
        return 1;

    size_t cap = b->cap ? b->cap : 128u;
    while (cap < need)
        cap *= 2u;

    next = realloc(b->buf, cap);
    if (!next)
        return 0;

    b->buf = next;
    b->cap = cap;
    return 1;
}

static int test_sbuf_putn(test_sbuf_t *b, const char *s, size_t n)
{
    if (!test_sbuf_reserve(b, n))
        return 0;

    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static int test_sbuf_puts(test_sbuf_t *b, const char *s)
{
    return test_sbuf_putn(b, s, strlen(s));
}

static int test_sbuf_putc(test_sbuf_t *b, char c)
{
    return test_sbuf_putn(b, &c, 1u);
}

static char *test_sbuf_steal(test_sbuf_t *b)
{
    char *out;

    if (!b->buf) {
        b->buf = malloc(1u);
        if (!b->buf)
            return NULL;
        b->buf[0] = '\0';
    }

    out = b->buf;
    b->buf = NULL;
    b->len = 0u;
    b->cap = 0u;
    return out;
}

static char *test_copy_range(const char *first, const char *last)
{
    size_t n = (size_t)(last - first);
    char *out = malloc(n + 1u);

    if (!out)
        return NULL;

    memcpy(out, first, n);
    out[n] = '\0';
    return out;
}

static const char *test_skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static char *test_trim_copy_range(const char *first, const char *last)
{
    while (first < last && (*first == ' ' || *first == '\t'))
        ++first;
    while (last > first && (last[-1] == ' ' || last[-1] == '\t'))
        --last;
    return test_copy_range(first, last);
}

static char *test_c_style_body_from_legacy(const char *body)
{
    test_sbuf_t out = {0};

    for (const char *p = body; *p; ++p) {
        if (*p == '*' || *p == '/') {
            if (!test_sbuf_puts(&out, *p == '*' ? " * " : " / ")) {
                free(out.buf);
                return NULL;
            }
        } else if (!test_sbuf_putc(&out, *p)) {
            free(out.buf);
            return NULL;
        }
    }

    return test_sbuf_steal(&out);
}

typedef struct {
    char *name;
    char *value;
    int is_const;
} test_legacy_binding_t;

static int test_legacy_binding_is_const_name(const char *name, const char *value)
{
    if (!name)
        return 0;

    if (strcmp(name, "π") == 0 || strcmp(name, "τ") == 0)
        return 1;
    if (strcmp(name, "e") == 0)
        return value && strncmp(value, "2.718281828", 11u) == 0;
    if (strcmp(name, "[pi]") == 0 ||
        strcmp(name, "[tau]") == 0 ||
        strcmp(name, "[2pi]") == 0)
        return 1;
    if (strncmp(name, "c", 1u) == 0 && strstr(name, "\xE2\x82"))
        return 1;

    return 0;
}

static int test_legacy_arg_is_const(const char *name,
                                    const test_legacy_binding_t *bindings,
                                    size_t nbindings)
{
    for (size_t i = 0u; i < nbindings; ++i) {
        if (bindings[i].is_const && strcmp(bindings[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static int test_emit_c_arg_list(test_sbuf_t *out,
                                const char *args,
                                int typed,
                                const test_legacy_binding_t *bindings,
                                size_t nbindings)
{
    const char *p = args;
    int first = 1;

    if (!args || !*args)
        return test_sbuf_puts(out, typed ? "void" : "");

    while (*p) {
        const char *start = p;
        const char *end;
        char *name;

        while (*p && *p != ',')
            ++p;
        end = p;

        name = test_trim_copy_range(start, end);
        if (!name)
            return 0;

        if (!first && !test_sbuf_puts(out, ", ")) {
            free(name);
            return 0;
        }
        if (typed && test_legacy_arg_is_const(name, bindings, nbindings) &&
            !test_sbuf_puts(out, "const ")) {
            free(name);
            return 0;
        }
        if (!test_sbuf_puts(out, name)) {
            free(name);
            return 0;
        }

        free(name);
        first = 0;
        if (*p == ',')
            ++p;
        p = test_skip_space(p);
    }

    return 1;
}

static char *test_legacy_function_expect_to_c(const char *legacy)
{
    test_legacy_binding_t bindings[64];
    size_t nbindings = 0u;
    const char *line = legacy;
    char *args = NULL;
    char *body = NULL;
    char *body_c = NULL;
    test_sbuf_t out = {0};

    memset(bindings, 0, sizeof(bindings));

    while (line && *line) {
        const char *eol = strchr(line, '\n');
        const char *end = eol ? eol : line + strlen(line);

        if ((size_t)(end - line) >= 5u && memcmp(line, "expr(", 5u) == 0) {
            const char *args_start = line + 5;
            const char *args_end = strstr(args_start, ") = ");
            if (!args_end)
                goto fail;
            args = test_copy_range(args_start, args_end);
            body = test_copy_range(args_end + 4, end);
            if (!args || !body)
                goto fail;
        } else if ((size_t)(end - line) >= 6u && memcmp(line, "return", 6u) == 0) {
            /* Rebuilt from the parsed expr() signature. */
        } else {
            const char *eq = strstr(line, " = ");
            if (eq && eq < end && nbindings < sizeof(bindings) / sizeof(bindings[0])) {
                bindings[nbindings].name = test_trim_copy_range(line, eq);
                bindings[nbindings].value = test_trim_copy_range(eq + 3, end);
                if (!bindings[nbindings].name || !bindings[nbindings].value)
                    goto fail;
                bindings[nbindings].is_const =
                    test_legacy_binding_is_const_name(bindings[nbindings].name,
                                                      bindings[nbindings].value);
                ++nbindings;
            }
        }

        line = eol ? eol + 1 : end;
    }

    if (!body)
        goto fail;

    body_c = test_c_style_body_from_legacy(body);
    if (!body_c)
        goto fail;

    if (!test_sbuf_puts(&out, "variable expr(") ||
        !test_emit_c_arg_list(&out, args, 1, bindings, nbindings) ||
        !test_sbuf_puts(&out, ") {\n") ||
        !test_sbuf_puts(&out, "    return ") ||
        !test_sbuf_puts(&out, body_c) ||
        !test_sbuf_puts(&out, ";\n}\n\nvariable expr_eval() {\n"))
        goto fail;

    for (size_t i = 0; i < nbindings; ++i) {
        if (!test_sbuf_puts(&out, "    ") ||
            (bindings[i].is_const && !test_sbuf_puts(&out, "const ")) ||
            !test_sbuf_puts(&out, bindings[i].name) ||
            !test_sbuf_puts(&out, " = ") ||
            !test_sbuf_puts(&out, bindings[i].value) ||
            !test_sbuf_puts(&out, ";\n"))
            goto fail;
    }

    if (!test_sbuf_puts(&out, "    return expr(") ||
        !test_emit_c_arg_list(&out, args, 0, bindings, nbindings) ||
        !test_sbuf_puts(&out, ");\n}"))
        goto fail;

    for (size_t i = 0; i < nbindings; ++i) {
        free(bindings[i].name);
        free(bindings[i].value);
    }
    free(args);
    free(body);
    free(body_c);
    return test_sbuf_steal(&out);

fail:
    for (size_t i = 0; i < nbindings; ++i) {
        free(bindings[i].name);
        free(bindings[i].value);
    }
    free(args);
    free(body);
    free(body_c);
    free(out.buf);
    return NULL;
}

static void test_to_string_basic_const_expr(void)
{
    dval_t *c = test_dv_new_const_d(3.5);
    char *got = dv_to_string(c, style_EXPRESSION);

    const char *expect = "3.5";

    if (str_eq(got, expect))
        to_string_pass("basic const (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (EXPR)", got, expect);

    free(got);
    dv_free(c);
}

static void test_to_string_basic_const_func(void)
{
    dval_t *c = test_dv_new_const_d(3.5);
    char *got = dv_to_string(c, style_FUNCTION);

    const char *expect = "variable expr(void) {\n"
                         "    return 3.5;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    return expr();\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("basic const (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (FUNC)", got, expect);

    free(got);
    dv_free(c);
}

void test_to_string_basic_const(void)
{
    TEST_RUN_SUBTEST(test_to_string_basic_const_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_const_func, NULL);
}

/* ============================================================
 * BASIC VAR
 * ============================================================ */

static void test_to_string_basic_var_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(42.0, "x");
    char *got = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 42 }";

    if (str_eq(got, expect))
        to_string_pass("basic var (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (EXPR)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_basic_var_func(void)
{
    dval_t *x = test_dv_new_named_var_d(42.0, "x");
    char *got = dv_to_string(x, style_FUNCTION);

    const char *expect = "variable expr(x) {\n"
                         "    return x;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 42;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("basic var (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (FUNC)", got, expect);

    free(got);
    dv_free(x);
}

void test_to_string_basic_var(void)
{
    TEST_RUN_SUBTEST(test_to_string_basic_var_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var_func, NULL);
}

static void test_to_string_basic_var_tex(void)
{
    dval_t *x = test_dv_new_named_var_d(42.0, "x0");
    char *got = dv_to_string(x, style_TEX);

    const char *expect = "\\left\\{ x_{0} \\;\\middle|\\; x_{0} = 42 \\right\\}";

    tex_preview_emit_case(__FILE__, "basic var (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("basic var (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (TEX)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_nested_transcendental_tex(void)
{
    dval_t *x = test_dv_new_named_var_d(1.0, "x0");
    dval_t *y = test_dv_new_named_var_d(2.0, "y1");
    dval_t *xy = dv_mul(x, y);
    dval_t *sin_xy = dv_sin(xy);
    dval_t *exp_term = dv_exp(sin_xy);
    dval_t *log_y = dv_log(y);
    dval_t *x_log_y = dv_mul(x, log_y);
    dval_t *f = dv_add(exp_term, x_log_y);
    char *got = dv_to_string(f, style_TEX);

    const char *expect =
        "\\left\\{ e^{\\sin(x_{0} y_{1})} + x_{0} \\cdot \\ln(y_{1}) "
        "\\;\\middle|\\; x_{0} = 1, y_{1} = 2 \\right\\}";

    tex_preview_emit_case(__FILE__, "nested transcendental (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("nested transcendental (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested transcendental (TEX)", got, expect);

    free(got);
    dv_free(f);
    dv_free(x_log_y);
    dv_free(log_y);
    dv_free(exp_term);
    dv_free(sin_xy);
    dv_free(xy);
    dv_free(x);
    dv_free(y);
}

static void test_to_string_nested_quotient_pow_tex(void)
{
    dval_t *x = test_dv_new_named_var_d(2.0, "x0");
    dval_t *y = test_dv_new_named_var_d(3.0, "y1");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *sum = dv_add(x2, y2);
    dval_t *den = dv_add_d(y, 1.0);
    dval_t *frac = dv_div(sum, den);
    dval_t *f = dv_log(frac);
    char *got = dv_to_string(f, style_TEX);

    const char *expect =
        "\\left\\{ \\ln(\\frac{x_{0}^{2} + y_{1}^{2}}{y_{1} + 1}) "
        "\\;\\middle|\\; x_{0} = 2, y_{1} = 3 \\right\\}";

    tex_preview_emit_case(__FILE__, "nested quotient pow (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("nested quotient pow (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested quotient pow (TEX)", got, expect);

    free(got);
    dv_free(f);
    dv_free(frac);
    dv_free(den);
    dv_free(sum);
    dv_free(y2);
    dv_free(x2);
    dv_free(x);
    dv_free(y);
}

static void test_to_string_log10_tex(void)
{
    dval_t *x = test_dv_new_named_var_d(100.0, "x0");
    dval_t *f = dv_log10(x);
    char *got = dv_to_string(f, style_TEX);

    const char *expect =
        "\\left\\{ \\log(x_{0}) \\;\\middle|\\; x_{0} = 100 \\right\\}";

    tex_preview_emit_case(__FILE__, "log10 (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("log10 (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log10 (TEX)", got, expect);

    free(got);
    dv_free(f);
    dv_free(x);
}

static void test_to_string_symbolic_constants_tex(void)
{
    dval_t *f = dval_from_string("{ exp(@pi*i*3/2*x) }", NULL);
    char *got = f ? dv_to_string(f, style_TEX) : NULL;

    const char *expect =
        "\\left\\{ e^{\\pi i \\cdot \\frac{3}{2} x} \\;\\middle|\\; x = NAN \\right\\}";

    tex_preview_emit_case(__FILE__, "symbolic constants (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("symbolic constants (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "symbolic constants (TEX)", got, expect);

    free(got);
    dv_free(f);
}

static void test_to_string_symbolic_constant_quotient_tex(void)
{
    dval_t *f = dval_from_string("{ 1/pi }", NULL);
    char *got = f ? dv_to_string(f, style_TEX) : NULL;

    const char *expect = "\\frac{1}{\\pi}";

    tex_preview_emit_case(__FILE__, "symbolic constant quotient (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("symbolic constant quotient (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1,
                       "symbolic constant quotient (TEX)", got, expect);

    free(got);
    dv_free(f);
}

static void test_to_string_lambert_w_tex(void)
{
    dval_t *x0 = test_dv_new_named_var_d(1.0, "x0");
    dval_t *x1 = test_dv_new_named_var_s("-0.2", "x1");
    dval_t *w0 = dv_lambert_w0(x0);
    dval_t *wm1 = dv_lambert_wm1(x1);
    char *got_w0 = dv_to_string(w0, style_TEX);
    char *got_wm1 = dv_to_string(wm1, style_TEX);

    const char *expect_w0 =
        "\\left\\{ W_{0}(x_{0}) \\;\\middle|\\; x_{0} = 1 \\right\\}";
    const char *expect_wm1 =
        "\\left\\{ W_{-1}(x_{1}) \\;\\middle|\\; x_{1} = -0.2 \\right\\}";

    tex_preview_emit_case(__FILE__, "lambert W0 (TEX)", got_w0);
    tex_preview_emit_case(__FILE__, "lambert W-1 (TEX)", got_wm1);

    if (str_eq(got_w0, expect_w0))
        to_string_pass("lambert W0 (TEX)", got_w0, expect_w0);
    else
        to_string_fail(__FILE__, __LINE__, 1, "lambert W0 (TEX)", got_w0, expect_w0);

    if (str_eq(got_wm1, expect_wm1))
        to_string_pass("lambert W-1 (TEX)", got_wm1, expect_wm1);
    else
        to_string_fail(__FILE__, __LINE__, 1, "lambert W-1 (TEX)", got_wm1, expect_wm1);

    free(got_w0);
    free(got_wm1);
    dv_free(w0);
    dv_free(wm1);
    dv_free(x0);
    dv_free(x1);
}

static void test_to_string_non_simple_var_bracketed_expr(void)
{
    dval_t *v = test_dv_new_named_var_d(42.0, "a0b0");
    char *got = dv_to_string(v, style_EXPRESSION);

    const char *expect = "{ [a0b₀] | [a0b₀] = 42 }";

    if (str_eq(got, expect))
        to_string_pass("non-simple var bracketed (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "non-simple var bracketed (EXPR)", got, expect);

    free(got);
    dv_free(v);
}

static void test_to_string_non_simple_var_bracketed_func(void)
{
    dval_t *v = test_dv_new_named_var_d(42.0, "a0b0");
    char *got = dv_to_string(v, style_FUNCTION);

    const char *expect = "variable expr([a0b₀]) {\n"
                         "    return [a0b₀];\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    [a0b₀] = 42;\n"
                         "    return expr([a0b₀]);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("non-simple var bracketed (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "non-simple var bracketed (FUNC)", got, expect);

    free(got);
    dv_free(v);
}

void test_to_string_non_simple_var_bracketed(void)
{
    TEST_RUN_SUBTEST(test_to_string_non_simple_var_bracketed_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_non_simple_var_bracketed_func, NULL);
}

/* ============================================================
 * ADDITION
 * ============================================================ */

static void test_to_string_addition_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(1, "x");
    dval_t *y = test_dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 1, y = 2 }";

    if (str_eq(got, expect))
        to_string_pass("addition (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

static void test_to_string_addition_func(void)
{
    dval_t *x = test_dv_new_named_var_d(1, "x");
    dval_t *y = test_dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x, y) {\n"
                         "    return x + y;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 1;\n"
                         "    y = 2;\n"
                         "    return expr(x, y);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("addition (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

void test_to_string_addition(void)
{
    TEST_RUN_SUBTEST(test_to_string_addition_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_addition_func, NULL);
}

static void test_to_string_negative_rhs_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");
    dval_t *z = test_dv_new_named_var_d(4, "z");
    dval_t *neg_y = dv_neg(y);
    dval_t *frac = dv_div(neg_y, z);
    dval_t *f = dv_sub(x, frac);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y/z | x = 2, y = 3, z = 4 }";

    if (str_eq(got, expect))
        to_string_pass("negative rhs quotient (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative rhs quotient (EXPR)", got, expect);

    free(got);
    dv_free(neg_y);
    dv_free(frac);
    dv_free(f);
    dv_free(x);
    dv_free(y);
    dv_free(z);
}

static void test_to_string_double_negative_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");
    dval_t *neg_y = dv_neg(y);
    dval_t *f = dv_sub(x, neg_y);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("double negative rhs (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "double negative rhs (EXPR)", got, expect);

    free(got);
    dv_free(neg_y);
    dv_free(f);
    dv_free(x);
    dv_free(y);
}

static void test_to_string_nested_negative_rhs_expr(void)
{
    dval_t *a = test_dv_new_named_var_d(5, "a");
    dval_t *b = test_dv_new_named_var_d(6, "b");
    dval_t *one = test_dv_new_const_d(1);
    dval_t *two = test_dv_new_const_d(2);
    dval_t *two_over_a = dv_div(two, a);
    dval_t *inner = dv_sub(one, two_over_a);
    dval_t *neg_inner = dv_neg(inner);
    dval_t *neg_inner_over_a = dv_div(neg_inner, a);
    dval_t *minus_two = test_dv_new_const_d(-2);
    dval_t *rhs = dv_div(neg_inner_over_a, b);
    dval_t *lhs = dv_div(minus_two, a);
    dval_t *f = dv_sub(lhs, rhs);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ -2/a + (1 - 2/a)/a/b | a = 5, b = 6 }";

    if (str_eq(got, expect))
        to_string_pass("nested negative rhs quotient (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested negative rhs quotient (EXPR)", got, expect);

    free(got);
    dv_free(f);
    dv_free(lhs);
    dv_free(rhs);
    dv_free(minus_two);
    dv_free(neg_inner_over_a);
    dv_free(neg_inner);
    dv_free(inner);
    dv_free(two_over_a);
    dv_free(two);
    dv_free(one);
    dv_free(a);
    dv_free(b);
}

/* ============================================================
 * NESTED MUL + ADD
 * ============================================================ */

static void test_to_string_nested_mul_add_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");
    dval_t *z = test_dv_new_named_var_d(4, "z");

    dval_t *xy = dv_mul(x, y);
    dval_t *f  = dv_add(xy, z);
    dval_t *simp = dv_simplify(f);

    char *got = dv_to_string(simp, style_EXPRESSION);
    const char *expect = "{ z + xy | z = 4, x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (EXPR)", got, expect);

    free(got);
    dv_free(simp);
    dv_free(xy);
    dv_free(x);
    dv_free(y);
    dv_free(z);
    dv_free(f);
}

static void test_to_string_nested_mul_add_func(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");
    dval_t *z = test_dv_new_named_var_d(4, "z");

    dval_t *xy = dv_mul(x, y);
    dval_t *f  = dv_add(xy, z);
    dval_t *simp = dv_simplify(f);

    char *got = dv_to_string(simp, style_FUNCTION);
    const char *expect = "variable expr(z, x, y) {\n"
                         "    return z + x * y;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    z = 4;\n"
                         "    x = 2;\n"
                         "    y = 3;\n"
                         "    return expr(z, x, y);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (FUNC)", got, expect);

    free(got);
    dv_free(simp);
    dv_free(xy);
    dv_free(x);
    dv_free(y);
    dv_free(z);
    dv_free(f);
}

void test_to_string_nested_mul_add(void)
{
    TEST_RUN_SUBTEST(test_to_string_nested_mul_add_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_mul_add_func, NULL);
}

static void test_to_string_atan2_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ atan2(x, y) | x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("atan2 (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

static void test_to_string_atan2_func(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *y = test_dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x, y) {\n"
                         "    return atan2(x, y);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 2;\n"
                         "    y = 3;\n"
                         "    return expr(x, y);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("atan2 (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

void test_to_string_atan2(void)
{
    TEST_RUN_SUBTEST(test_to_string_atan2_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_atan2_func, NULL);
}

/* ============================================================
 * POW SUPERSCRIPT
 * ============================================================ */

static void test_to_string_pow_superscript_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x³ | x = 2 }";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_pow_superscript_func(void)
{
    dval_t *x = test_dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x) {\n"
                         "    return x^3;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 2;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_complex_const_pow_expr(void)
{
    number_t n = num_create_from_string("1 + 2i");
    dval_t *base = dv_new_const(n);
    dval_t *pow = dv_pow_d(base, 6.0);
    dval_t *f = dv_add_d(pow, 1.0);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "(1 + 2i)⁶ + 1";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (EXPR)", got, expect);

    free(got);
    dv_free(f);
    dv_free(pow);
    dv_free(base);
    num_destroy(&n);
}

static void test_to_string_complex_const_pow_func(void)
{
    number_t n = num_create_from_string("1 + 2i");
    dval_t *base = dv_new_const(n);
    dval_t *pow = dv_pow_d(base, 6.0);
    dval_t *f = dv_add_d(pow, 1.0);
    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "variable expr(void) {\n"
        "    return (1 + 2i)^6 + 1;\n"
        "}\n"
        "\n"
        "variable expr_eval() {\n"
        "    return expr();\n"
        "}";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (FUNC)", got, expect);

    free(got);
    dv_free(f);
    dv_free(pow);
    dv_free(base);
    num_destroy(&n);
}

static void test_to_string_complex_const_pow_tex(void)
{
    number_t n = num_create_from_string("1 + 2i");
    dval_t *base = dv_new_const(n);
    dval_t *pow = dv_pow_d(base, 6.0);
    dval_t *f = dv_add_d(pow, 1.0);
    char *got = dv_to_string(f, style_TEX);
    const char *expect =
        "\\left(1 + 2i\\right)^{6} + 1";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (TEX)", got, expect);

    free(got);
    dv_free(f);
    dv_free(pow);
    dv_free(base);
    num_destroy(&n);
}

static void test_to_string_parsed_complex_const_pow_tex(void)
{
    dval_t *f = dval_from_string("{ (1 + 2i)^6 + 1 }", NULL);
    char *got = f ? dv_to_string(f, style_TEX) : NULL;
    const char *expect =
        "\\left(1 + 2i\\right)^{6} + 1";

    if (str_eq(got, expect))
        to_string_pass("parsed complex const power base (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "parsed complex const power base (TEX)", got, expect);

    free(got);
    dv_free(f);
}

static void test_to_string_parsed_complex_const_pow_expr(void)
{
    dval_t *f = dval_from_string("{ (1 + 2i)^6 + 1 }", NULL);
    char *got = f ? dv_to_string(f, style_EXPRESSION) : NULL;
    const char *expect = "(1 + 2i)⁶ + 1";

    if (str_eq(got, expect))
        to_string_pass("parsed complex const power base (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "parsed complex const power base (EXPR)", got, expect);

    free(got);
    dv_free(f);
}

void test_to_string_pow_superscript(void)
{
    TEST_RUN_SUBTEST(test_to_string_pow_superscript_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_pow_superscript_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_parsed_complex_const_pow_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_parsed_complex_const_pow_expr, NULL);
}

/* ============================================================
 * UNARY SIN
 * ============================================================ */

static void test_to_string_unary_sin_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ sin(x) | x = 0.5 }";

    if (str_eq(got, expect))
        to_string_pass("unary sin (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_unary_sin_func(void)
{
    dval_t *x = test_dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x) {\n"
                         "    return sin(x);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 0.5;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("unary sin (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

void test_to_string_unary_sin(void)
{
    TEST_RUN_SUBTEST(test_to_string_unary_sin_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_unary_sin_func, NULL);
}

static void test_to_string_unary_sqrt_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(4.0, "x");
    dval_t *f = dv_sqrt(x);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ √(x) | x = 4 }";

    if (str_eq(got, expect))
        to_string_pass("unary sqrt (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sqrt (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_unary_sqrt_func(void)
{
    dval_t *x = test_dv_new_named_var_d(4.0, "x");
    dval_t *f = dv_sqrt(x);
    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x) {\n"
                         "    return sqrt(x);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 4;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("unary sqrt (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sqrt (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

void test_to_string_unary_sqrt(void)
{
    TEST_RUN_SUBTEST(test_to_string_unary_sqrt_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_unary_sqrt_func, NULL);
}

/* ============================================================
 * FUNCTION STYLE (identity)
 * ============================================================ */

static void test_to_string_function_style_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(10, "x");
    char *got = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 10 }";

    if (str_eq(got, expect))
        to_string_pass("function style identity (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (EXPR)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_function_style_func(void)
{
    dval_t *x = test_dv_new_named_var_d(10, "x");
    char *got = dv_to_string(x, style_FUNCTION);

    const char *expect = "variable expr(x) {\n"
                         "    return x;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 10;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("function style identity (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (FUNC)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_function_style_signed_sum(void)
{
    dval_t *x = test_dv_new_named_var_d(1, "x");
    dval_t *three = test_dv_new_const_d(3);
    dval_t *minus_seven = test_dv_new_const_d(-7);
    dval_t *sin_x = dv_sin(x);
    dval_t *exp_sin_x = dv_exp(sin_x);
    dval_t *x2 = dv_mul(x, x);
    dval_t *three_x2 = dv_mul(three, x2);
    dval_t *sum = dv_add(exp_sin_x, three_x2);
    dval_t *f = dv_add(sum, minus_seven);
    dval_t *simp = dv_simplify(f);
    char *got = dv_to_string(simp, style_FUNCTION);

    const char *expect = "variable expr(x) {\n"
                         "    return exp(sin(x)) + 3 * x^2 - 7;\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 1;\n"
                         "    return expr(x);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("function style signed sum (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style signed sum (FUNC)", got, expect);

    free(got);
    dv_free(simp);
    dv_free(x);
    dv_free(three);
    dv_free(minus_seven);
    dv_free(sin_x);
    dv_free(exp_sin_x);
    dv_free(x2);
    dv_free(three_x2);
    dv_free(sum);
    dv_free(f);
}

static void test_to_string_function_style_sub_negative_product(void)
{
    dval_t *E = test_dv_new_named_var_d(0.8, "E");
    dval_t *ecc = test_dv_new_named_var_d(0.0167, "e");
    dval_t *sin_E = dv_sin(E);
    dval_t *ecc_sin_E = dv_mul(ecc, sin_E);
    dval_t *f = dv_sub(E, ecc_sin_E);
    char *got = dv_to_string(f, style_FUNCTION);

    const char *expect = "variable expr(E, e) {\n"
                         "    return E - e * sin(E);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    E = 0.8000000000000000444089209850062616;\n"
                         "    e = 0.01669999999999999956701302039618894;\n"
                         "    return expr(E, e);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("function style negative product rhs (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style negative product rhs (FUNC)", got, expect);

    free(got);
    dv_free(f);
    dv_free(ecc_sin_E);
    dv_free(sin_E);
    dv_free(ecc);
    dv_free(E);
}

static void test_to_string_function_style_preserves_math_names(void)
{
    dval_t *f = dval_from_string("{ tan(x*y*c0/2) | x = 3.25, y = 4.5; c0 = gamma }", NULL);
    char *got = dv_to_string(f, style_FUNCTION);

    const char *expect = "variable expr(x, y, const c₀) {\n"
                         "    return tan(x * y * c₀ / 2);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 3.25;\n"
                         "    y = 4.5;\n"
                         "    const c₀ = γ;\n"
                         "    return expr(x, y, c₀);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("function style preserves math names (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style preserves math names (FUNC)", got, expect);

    free(got);
    dv_free(f);
}

void test_to_string_function_style(void)
{
    TEST_RUN_SUBTEST(test_to_string_function_style_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_signed_sum, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_sub_negative_product, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_preserves_math_names, NULL);
}

static void test_to_string_floor_ceil_expr(void)
{
    dval_t *x = test_dv_new_named_var_d(1.5, "x");
    dval_t *y = test_dv_new_named_var_d(-1.5, "y");
    dval_t *floor_x = dv_floor(x);
    dval_t *ceil_y = dv_ceil(y);
    dval_t *f = dv_add(floor_x, ceil_y);
    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ ⌊x⌋ + ⌈y⌉ | x = 1.5, y = -1.5 }";

    if (str_eq(got, expect))
        to_string_pass("floor/ceil mathematical notation (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "floor/ceil mathematical notation (EXPR)", got, expect);

    free(got);
    dv_free(f);
    dv_free(ceil_y);
    dv_free(floor_x);
    dv_free(y);
    dv_free(x);
}

static void test_to_string_floor_ceil_func(void)
{
    dval_t *x = test_dv_new_named_var_d(1.5, "x");
    dval_t *y = test_dv_new_named_var_d(-1.5, "y");
    dval_t *floor_x = dv_floor(x);
    dval_t *ceil_y = dv_ceil(y);
    dval_t *f = dv_add(floor_x, ceil_y);
    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "variable expr(x, y) {\n"
                         "    return floor(x) + ceil(y);\n"
                         "}\n"
                         "\n"
                         "variable expr_eval() {\n"
                         "    x = 1.5;\n"
                         "    y = -1.5;\n"
                         "    return expr(x, y);\n"
                         "}";

    if (str_eq(got, expect))
        to_string_pass("floor/ceil function notation (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "floor/ceil function notation (FUNC)", got, expect);

    free(got);
    dv_free(f);
    dv_free(ceil_y);
    dv_free(floor_x);
    dv_free(y);
    dv_free(x);
}

void test_to_string_floor_ceil(void)
{
    TEST_RUN_SUBTEST(test_to_string_floor_ceil_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_floor_ceil_func, NULL);
}

/* ============================================================
 * SPECIAL FUNCTIONS — round-trip for all 18 new ops
 * ============================================================ */

/* check_roundtrip is defined later in the from_string section */
void check_roundtrip(const char *label, dval_t *f, int line);

void test_to_string_special_functions(void)
{
    /* Unary functions */
    { dval_t *x = test_dv_new_named_var_d(-3.0, "x"); check_roundtrip("to_string: abs(x)",           dv_abs(x),           __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.5, "x"); check_roundtrip("to_string: floor(x)",         dv_floor(x),         __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.5, "x"); check_roundtrip("to_string: ceil(x)",          dv_ceil(x),          __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erf(x)",           dv_erf(x),           __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfc(x)",          dv_erfc(x),          __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfinv(x)",        dv_erfinv(x),        __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfcinv(x)",       dv_erfcinv(x),       __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 3.0, "x"); check_roundtrip("to_string: gamma(x)",         dv_gamma(x),         __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.329340388179137, "x"); check_roundtrip("to_string: gammainv(x)",      dv_gammainv(x),      __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 3.0, "x"); check_roundtrip("to_string: lgamma(x)",        dv_lgamma(x),        __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: digamma(x)",       dv_digamma(x),       __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: W₀(x)",            dv_lambert_w0(x),    __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d(-0.2, "x"); check_roundtrip("to_string: W₋₁(x)",           dv_lambert_wm1(x),   __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_pdf(x)",    dv_normal_pdf(x),    __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_cdf(x)",    dv_normal_cdf(x),    __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_logpdf(x)", dv_normal_logpdf(x), __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: ei(x)",            dv_ei(x),            __LINE__); dv_free(x); }
    { dval_t *x = test_dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: e1(x)",            dv_e1(x),            __LINE__); dv_free(x); }
    /* Binary functions */
    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *f = dv_beta(x, y);    dv_free(x); dv_free(y);
        check_roundtrip("to_string: beta(x,y)", f, __LINE__);
    }
    {
        dval_t *x = test_dv_new_named_var_d(2.0, "x");
        dval_t *y = test_dv_new_named_var_d(3.0, "y");
        dval_t *f = dv_logbeta(x, y); dv_free(x); dv_free(y);
        check_roundtrip("to_string: logbeta(x,y)", f, __LINE__);
    }
    {
        dval_t *x = test_dv_new_named_var_d(3.0, "x");
        dval_t *y = test_dv_new_named_var_d(4.0, "y");
        dval_t *f = dv_hypot(x, y);   dv_free(x); dv_free(y);
        check_roundtrip("to_string: hypot(x,y)", f, __LINE__);
    }
}

/* ============================================================
 * TEST SUITE RUNNER
 * ============================================================ */

void test_to_string_all(void)
{
    TEST_RUN_SUBTEST(test_to_string_basic_const, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_transcendental_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_quotient_pow_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_log10_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_symbolic_constants_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_symbolic_constant_quotient_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_lambert_w_tex, NULL);
    TEST_RUN_SUBTEST(test_to_string_non_simple_var_bracketed, NULL);
    TEST_RUN_SUBTEST(test_to_string_addition, NULL);
    TEST_RUN_SUBTEST(test_to_string_negative_rhs_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_double_negative_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_negative_rhs_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_mul_add, NULL);
    TEST_RUN_SUBTEST(test_to_string_atan2, NULL);
    TEST_RUN_SUBTEST(test_to_string_pow_superscript, NULL);
    TEST_RUN_SUBTEST(test_to_string_unary_sin, NULL);
    TEST_RUN_SUBTEST(test_to_string_unary_sqrt, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style, NULL);
    TEST_RUN_SUBTEST(test_to_string_floor_ceil, NULL);
    TEST_RUN_SUBTEST(test_to_string_special_functions, NULL);
}

/* ============================================================
 *  make_expr_01        x*x
 * ============================================================ */
static dval_t *make_expr_01(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);   /* x*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_02        x*x*x
 * ============================================================ */
static dval_t *make_expr_02(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);   /* x*x      */
    dval_t *t2 = dv_mul(t1, x);  /* x*x*x    */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_03        π * x^2
 * ============================================================ */
static dval_t *make_expr_03(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2      */
    dval_t *t2 = dv_mul(pi, t1);     /* π * x^2  */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_04        x*x + x*x
 * ============================================================ */
static dval_t *make_expr_04(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);       /* x*x        */
    dval_t *t2 = dv_mul(x, x);       /* x*x        */
    dval_t *t3 = dv_add(t1, t2);     /* x*x + x*x  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_05        x*x + 3*x*x + 7
 * ============================================================ */
static dval_t *make_expr_05(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);        /* x*x          */
    dval_t *t2 = dv_mul_d(t1, 3.0);   /* 3*x*x        */
    dval_t *t3 = dv_add(t1, t2);      /* x*x+3*x*x    */
    dval_t *t4 = dv_add_d(t3, 7.0);   /* x*x+3*x*x+7  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_06        2*x - 5*x
 * ============================================================ */
static dval_t *make_expr_06(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 2.0);   /* 2*x   */
    dval_t *t2 = dv_mul_d(x, 5.0);   /* 5*x   */
    dval_t *t3 = dv_sub(t1, t2);     /* 2*x-5*x */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_07        x^2 * x^3
 * ============================================================ */
static dval_t *make_expr_07(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2      */
    dval_t *t2 = dv_pow_d(x, 3.0);   /* x^3      */
    dval_t *t3 = dv_mul(t1, t2);     /* x^2*x^3  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_08        x^2 * x * x^4
 * ============================================================ */
static dval_t *make_expr_08(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_mul(t1, x);      /* x^2*x      */
    dval_t *t3 = dv_pow_d(x, 4.0);   /* x^4        */
    dval_t *t4 = dv_mul(t2, t3);     /* x^2*x*x^4  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_09        x^2 * y^3 * x
 * ============================================================ */
static dval_t *make_expr_09(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *y = test_dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_pow_d(y, 3.0);   /* y^3        */
    dval_t *t3 = dv_mul(t1, t2);     /* x^2*y^3    */
    dval_t *t4 = dv_mul(t3, x);      /* x^2*y^3*x  */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_10        3*x^2 * 4*x
 * ============================================================ */
static dval_t *make_expr_10(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);    /* x^2        */
    dval_t *t2 = dv_mul_d(t1, 3.0);   /* 3*x^2      */
    dval_t *t3 = dv_mul_d(x, 4.0);    /* 4*x        */
    dval_t *t4 = dv_mul(t2, t3);      /* 3*x^2*4*x  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_11        3*x * 2*y * x^2   → 6 x^3 y
 * ============================================================ */
static dval_t *make_expr_11(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *y = test_dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul_d(x, 3.0);      /* 3*x     */
    dval_t *t2 = dv_mul_d(y, 2.0);      /* 2*y     */
    dval_t *t3 = dv_mul(t1, t2);        /* 3*x*2*y */
    dval_t *t4 = dv_pow_d(x, 2.0);      /* x^2     */
    dval_t *t5 = dv_mul(t3, t4);        /* 3*x*2*y*x^2 */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_12        x*x*y*x   → x^3 y
 * ============================================================ */
static dval_t *make_expr_12(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *y = test_dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul(x, x);      /* x*x     */
    dval_t *t2 = dv_mul(t1, y);     /* x*x*y   */
    dval_t *t3 = dv_mul(t2, x);     /* x*x*y*x */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_13        3*x
 * ============================================================ */
static dval_t *make_expr_13(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 3.0);   /* 3*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_14        3*x*x
 * ============================================================ */
static dval_t *make_expr_14(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);       /* x*x   */
    dval_t *t2 = dv_mul_d(t1, 3.0);  /* 3*x*x */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_15        6*x
 * ============================================================ */
static dval_t *make_expr_15(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 6.0);   /* 6*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_16        7*x^2
 * ============================================================ */
static dval_t *make_expr_16(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);    /* x^2     */
    dval_t *t2 = dv_mul_d(t1, 7.0);   /* 7*x^2   */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_17        2*x*y
 * ============================================================ */
static dval_t *make_expr_17(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *y = test_dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul(x, y);        /* x*y     */
    dval_t *t2 = dv_mul_d(t1, 2.0);   /* 2*x*y   */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_18        sin(x) * cos(x)
 * ============================================================ */
static dval_t *make_expr_18(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(x);       /* cos(x) */
    dval_t *t3 = dv_mul(t1, t2);  /* sin(x)*cos(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_19        cos(x) * exp(x)
 * ============================================================ */
static dval_t *make_expr_19(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_cos(x);       /* cos(x) */
    dval_t *t2 = dv_exp(x);       /* exp(x) */
    dval_t *t3 = dv_mul(t1, t2);  /* cos(x)*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_20        exp(x) * x*x   → x^2 * exp(x)
 * ============================================================ */
static dval_t *make_expr_20(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);       /* exp(x) */
    dval_t *t2 = dv_mul(x, x);    /* x*x    */
    dval_t *t3 = dv_mul(t2, t1);  /* x*x*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_21        3*exp(x) * x^2
 * ============================================================ */
static dval_t *make_expr_21(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);          /* exp(x)     */
    dval_t *t2 = dv_mul_d(t1, 3.0);  /* 3*exp(x)   */
    dval_t *t3 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t4 = dv_mul(t2, t3);     /* 3*exp(x)*x^2 */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_22        sin(x) * x^2
 * ============================================================ */
static dval_t *make_expr_22(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);          /* sin(x) */
    dval_t *t2 = dv_pow_d(x, 2.0);   /* x^2    */
    dval_t *t3 = dv_mul(t1, t2);     /* sin(x)*x^2 */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_23        x*sin(x)*x   → x^2 * sin(x)
 * ============================================================ */
static dval_t *make_expr_23(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_mul(x, t1);   /* x*sin(x) */
    dval_t *t3 = dv_mul(t2, x);   /* x*sin(x)*x */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_24        exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_24(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_25        cos(x) * exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_25(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_cos(x);       /* cos(x) */
    dval_t *t2 = dv_sin(x);       /* sin(x) */
    dval_t *t3 = dv_exp(t2);      /* exp(sin(x)) */
    dval_t *t4 = dv_mul(t1, t3);  /* cos(x)*exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_26        x*x * exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_26(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);    /* x*x */
    dval_t *t2 = dv_sin(x);       /* sin(x) */
    dval_t *t3 = dv_exp(t2);      /* exp(sin(x)) */
    dval_t *t4 = dv_mul(t1, t3);  /* x*x*exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_27        exp(sin(x)) * exp(cos(x))
 * ============================================================ */
static dval_t *make_expr_27(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */
    dval_t *t3 = dv_cos(x);       /* cos(x) */
    dval_t *t4 = dv_exp(t3);      /* exp(cos(x)) */
    dval_t *t5 = dv_mul(t2, t4);  /* exp(sin(x))*exp(cos(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_28        exp(x^2) * exp(3*x^2)
 * ============================================================ */
static dval_t *make_expr_28(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);     /* x^2       */
    dval_t *t2 = dv_exp(t1);           /* exp(x^2)  */
    dval_t *t3 = dv_mul_d(t1, 3.0);    /* 3*x^2     */
    dval_t *t4 = dv_exp(t3);           /* exp(3*x^2) */
    dval_t *t5 = dv_mul(t2, t4);       /* exp(x^2)*exp(3*x^2) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_29        exp(x) * exp(2*x)
 * ============================================================ */
static dval_t *make_expr_29(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);          /* exp(x)   */
    dval_t *t2 = dv_mul_d(x, 2.0);   /* 2*x      */
    dval_t *t3 = dv_exp(t2);         /* exp(2*x) */
    dval_t *t4 = dv_mul(t1, t3);     /* exp(x)*exp(2*x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_30        exp(sin(x)) * exp(cos(x)) * exp(x)
 * ============================================================ */
static dval_t *make_expr_30(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */
    dval_t *t3 = dv_cos(x);       /* cos(x) */
    dval_t *t4 = dv_exp(t3);      /* exp(cos(x)) */
    dval_t *t5 = dv_exp(x);       /* exp(x) */
    dval_t *t6 = dv_mul(t2, t4);  /* exp(sin(x))*exp(cos(x)) */
    dval_t *t7 = dv_mul(t6, t5);  /* exp(sin(x))*exp(cos(x))*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    dv_free(t6);
    return t7;
}

/* ============================================================
 *  make_expr_31        π * sin(x)
 * ============================================================ */
static dval_t *make_expr_31(void)
{
    dval_t *x  = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");

    dval_t *t1 = dv_sin(x);       /* sin(x)   */
    dval_t *t2 = dv_mul(pi, t1);  /* π*sin(x) */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_32        τ * cos(x)
 * ============================================================ */
static dval_t *make_expr_32(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_cos(x);        /* cos(x)   */
    dval_t *t2 = dv_mul(tau, t1);  /* τ*cos(x) */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_33        e * x^2
 * ============================================================ */
static dval_t *make_expr_33(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *e = test_dv_new_named_const_qf(QF_E, "e");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2    */
    dval_t *t2 = dv_mul(e, t1);      /* e*x^2  */

    dv_free(x);
    dv_free(e);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_34        π * τ * e
 * ============================================================ */
static dval_t *make_expr_34(void)
{
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");
    dval_t *e   = test_dv_new_named_const_qf(QF_E, "e");

    dval_t *t1 = dv_mul(pi, tau);   /* π*τ   */
    dval_t *t2 = dv_mul(t1, e);     /* π*τ*e */

    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_35        π * x * τ * y
 * ============================================================ */
static dval_t *make_expr_35(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, x);      /* π*x     */
    dval_t *t2 = dv_mul(t1, tau);    /* π*x*τ   */
    dval_t *t3 = dv_mul(t2, y);      /* π*x*τ*y */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_36        e^(x) * π
 * ============================================================ */
static dval_t *make_expr_36(void)
{
    dval_t *x  = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");

    dval_t *t1 = dv_exp(x);       /* exp(x) */
    dval_t *t2 = dv_mul(t1, pi);  /* exp(x)*π */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_37        τ * exp(x^2)
 * ============================================================ */
static dval_t *make_expr_37(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_exp(t1);         /* exp(x^2)   */
    dval_t *t3 = dv_mul(tau, t2);    /* τ*exp(x^2) */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_38        e * sin(x) * cos(y)
 * ============================================================ */
static dval_t *make_expr_38(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *y = test_dv_new_named_var_d(1.25, "y");
    dval_t *e = test_dv_new_named_const_qf(QF_E, "e");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(y);       /* cos(y) */
    dval_t *t3 = dv_mul(t1, t2);  /* sin(x)*cos(y) */
    dval_t *t4 = dv_mul(e, t3);   /* e*sin(x)*cos(y) */

    dv_free(x);
    dv_free(y);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_39        π * exp(τ * x)
 * ============================================================ */
static dval_t *make_expr_39(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(tau, x);   /* τ*x        */
    dval_t *t2 = dv_exp(t1);       /* exp(τ*x)   */
    dval_t *t3 = dv_mul(pi, t2);   /* π*exp(τ*x) */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_40        e^(π*x) * τ
 * ============================================================ */
static dval_t *make_expr_40(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, x);   /* π*x      */
    dval_t *t2 = dv_exp(t1);      /* exp(π*x) */
    dval_t *t3 = dv_mul(t2, tau); /* exp(π*x)*τ */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_41        sin(π * x)
 * ============================================================ */
static dval_t *make_expr_41(void)
{
    dval_t *x  = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");

    dval_t *t1 = dv_mul(pi, x);   /* π*x       */
    dval_t *t2 = dv_sin(t1);      /* sin(π*x)  */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_42        cos(τ * x)
 * ============================================================ */
static dval_t *make_expr_42(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(tau, x);  /* τ*x       */
    dval_t *t2 = dv_cos(t1);      /* cos(τ*x)  */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_43        exp(π * τ * x)
 * ============================================================ */
static dval_t *make_expr_43(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, tau);  /* π*τ      */
    dval_t *t2 = dv_mul(t1, x);    /* π*τ*x    */
    dval_t *t3 = dv_exp(t2);       /* exp(π*τ*x) */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_44        sin(x) + cos(x) + exp(x)
 * ============================================================ */
static dval_t *make_expr_44(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(x);       /* cos(x) */
    dval_t *t3 = dv_add(t1, t2);  /* sin(x)+cos(x) */
    dval_t *t4 = dv_exp(x);       /* exp(x) */
    dval_t *t5 = dv_add(t3, t4);  /* sin(x)+cos(x)+exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_45        x + y + π + τ + e
 * ============================================================ */
static dval_t *make_expr_45(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");
    dval_t *e   = test_dv_new_named_const_qf(QF_E, "e");

    dval_t *t1 = dv_add(x, y);     /* x+y     */
    dval_t *t2 = dv_add(t1, pi);   /* x+y+π   */
    dval_t *t3 = dv_add(t2, tau);  /* x+y+π+τ */
    dval_t *t4 = dv_add(t3, e);    /* x+y+π+τ+e */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_46        x*y + π*x + τ*y + e
 * ============================================================ */
static dval_t *make_expr_46(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");
    dval_t *e   = test_dv_new_named_const_qf(QF_E, "e");

    dval_t *t1 = dv_mul(x, y);     /* x*y     */
    dval_t *t2 = dv_mul(pi, x);    /* π*x     */
    dval_t *t3 = dv_mul(tau, y);   /* τ*y     */
    dval_t *t4 = dv_add(t1, t2);   /* x*y + π*x */
    dval_t *t5 = dv_add(t4, t3);   /* x*y + π*x + τ*y */
    dval_t *t6 = dv_add(t5, e);    /* x*y + π*x + τ*y + e */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    return t6;
}

/* ============================================================
 *  make_expr_47        (x + π) * (y + τ)
 * ============================================================ */
static dval_t *make_expr_47(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_add(y, tau);    /* y+τ */
    dval_t *t3 = dv_mul(t1, t2);    /* (x+π)*(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_48        exp(x + π) * exp(y + τ)
 * ============================================================ */
static dval_t *make_expr_48(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_exp(t1);        /* exp(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_exp(t3);        /* exp(y+τ) */
    dval_t *t5 = dv_mul(t2, t4);    /* exp(x+π)*exp(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_49        sin(x + π) * cos(y + τ)
 * ============================================================ */
static dval_t *make_expr_49(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_sin(t1);        /* sin(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_cos(t3);        /* cos(y+τ) */
    dval_t *t5 = dv_mul(t2, t4);    /* sin(x+π)*cos(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_50        exp(sin(x + π) + cos(y + τ))
 * ============================================================ */
static dval_t *make_expr_50(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *y   = test_dv_new_named_var_d(1.25, "y");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_sin(t1);        /* sin(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_cos(t3);        /* cos(y+τ) */
    dval_t *t5 = dv_add(t2, t4);    /* sin(x+π)+cos(y+τ) */
    dval_t *t6 = dv_exp(t5);        /* exp(sin(x+π)+cos(y+τ)) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    return t6;
}

void test_expressions(void)
{
    /* ============================================================
     *  Test table (all 50 entries)
     * ============================================================ */
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;   /* NEW: source line of this test entry */
    } tests[] = {
        /* 01 */
        {
            "x*x",
            make_expr_01,
            "{ x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 02 */
        {
            "x*x*x",
            make_expr_02,
            "{ x³ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^3\n"
            "return expr(x)",
            __LINE__
        },

        /* 03 */
        {
            "π * x^2",
            make_expr_03,
            "{ πx² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = π*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 04 */
        {
            "x*x + x*x",
            make_expr_04,
            "{ 2x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 2*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 05 */
        {
            "x*x + 3*x*x + 7",
            make_expr_05,
            "{ 4x² + 7 | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 4*x^2 + 7\n"
            "return expr(x)",
            __LINE__
        },

        /* 06 */
        {
            "2*x - 5*x",
            make_expr_06,
            "{ -3x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = -3*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 07 */
        {
            "x^2 * x^3",
            make_expr_07,
            "{ x⁵ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^5\n"
            "return expr(x)",
            __LINE__
        },

        /* 08 */
        {
            "x^2 * x * x^4",
            make_expr_08,
            "{ x⁷ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^7\n"
            "return expr(x)",
            __LINE__
        },

        /* 09 */
        {
            "x^2 * y^3 * x",
            make_expr_09,
            "{ x³y³ | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = x^3*y^3\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 10 */
        {
            "3*x^2 * 4*x",
            make_expr_10,
            "{ 12x³ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 12*x^3\n"
            "return expr(x)",
            __LINE__
        },

        /* 11 */
        {
            "3*x * 2*y * x^2",
            make_expr_11,
            "{ 6x³y | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = 6*x^3*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 12 */
        {
            "x*x*y*x",
            make_expr_12,
            "{ x³y | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = x^3*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 13 */
        {
            "3*x",
            make_expr_13,
            "{ 3x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 14 */
        {
            "3*x*x",
            make_expr_14,
            "{ 3x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 15 */
        {
            "6*x",
            make_expr_15,
            "{ 6x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 6*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 16 */
        {
            "7*x^2",
            make_expr_16,
            "{ 7x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 7*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 17 */
        {
            "2*x*y",
            make_expr_17,
            "{ 2xy | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = 2*x*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 18 */
        {
            "sin(x)*cos(x)",
            make_expr_18,
            "{ sin(x)·cos(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(x)*cos(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 19 */
        {
            "cos(x)*exp(x)",
            make_expr_19,
            "{ cos(x)·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = cos(x)*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 20 */
        {
            "exp(x)*x*x",
            make_expr_20,
            "{ x²·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 21 */
        {
            "3*exp(x)*x^2",
            make_expr_21,
            "{ 3x²·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x^2*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 22 */
        {
            "sin(x)*x^2",
            make_expr_22,
            "{ x²·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*sin(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 23 */
        {
            "x*sin(x)*x",
            make_expr_23,
            "{ x²·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*sin(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 24 */
        {
            "exp(sin(x))",
            make_expr_24,
            "{ exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 25 */
        {
            "cos(x)*exp(sin(x))",
            make_expr_25,
            "{ cos(x)·exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = cos(x)*exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 26 */
        {
            "x*x*exp(sin(x))",
            make_expr_26,
            "{ x²·exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 27 */
        {
            "exp(sin(x))*exp(cos(x))",
            make_expr_27,
            "{ exp(sin(x) + cos(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x) + cos(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 28 */
        {
            "exp(x^2)*exp(3*x^2)",
            make_expr_28,
            "{ exp(4x²) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(4*x^2)\n"
            "return expr(x)",
            __LINE__
        },

        /* 29 */
        {
            "exp(x)*exp(2*x)",
            make_expr_29,
            "{ exp(3x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(3*x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 30 */
        {
            "exp(sin(x))*exp(cos(x))*exp(x)",
            make_expr_30,
            "{ exp(sin(x) + cos(x) + x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x) + cos(x) + x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 31 */
        {
            "π*sin(x)",
            make_expr_31,
            "{ π·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = π*sin(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 32 */
        {
            "τ*cos(x)",
            make_expr_32,
            "{ τ·cos(x) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = τ*cos(x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 33 */
        {
            "e*x^2",
            make_expr_33,
            "{ ex² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = e*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 34 */
        {
            "π*τ*e",
            make_expr_34,
            "{ πτe | ; τ = 6.283185307179586476925286766559011 }",
            "τ = 6.283185307179586476925286766559011\n"
            "expr(τ) = π*τ*e\n"
            "return expr(τ)",
            __LINE__
        },

        /* 35 */
        {
            "π*x*τ*y",
            make_expr_35,
            "{ πτxy | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = π*τ*x*y\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 36 */
        {
            "exp(x)*π",
            make_expr_36,
            "{ π·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = π*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 37 */
        {
            "τ*exp(x^2)",
            make_expr_37,
            "{ τ·exp(x²) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = τ*exp(x^2)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 38 */
        {
            "e*sin(x)*cos(y)",
            make_expr_38,
            "{ e·sin(x)·cos(y) | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = e*sin(x)*cos(y)\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 39 */
        {
            "π*exp(τ*x)",
            make_expr_39,
            "{ π·exp(τx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = π*exp(τ*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 40 */
        {
            "exp(π*x)*τ",
            make_expr_40,
            "{ τ·exp(πx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = τ*exp(π*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 41 */
        {
            "sin(π*x)",
            make_expr_41,
            "{ sin(πx) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(π*x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 42 */
        {
            "cos(τ*x)",
            make_expr_42,
            "{ cos(τx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = cos(τ*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 43 */
        {
            "exp(π*τ*x)",
            make_expr_43,
            "{ exp(πτx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = exp(π*τ*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 44 */
        {
            "sin(x)+cos(x)+exp(x)",
            make_expr_44,
            "{ sin(x) + cos(x) + exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(x) + cos(x) + exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 45 */
        {
            "x + y + π + τ + e",
            make_expr_45,
            "{ x + y + e + π + τ | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = x + y + e + π + τ\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 46 */
        {
            "x*y + π*x + τ*y + e",
            make_expr_46,
            "{ e + πx + τy + xy | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = e + π*x + τ*y + x*y\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 47 */
        {
            "(x+π)*(y+τ)",
            make_expr_47,
            "{ (x + π)·(y + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = (x + π)*(y + τ)\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 48 */
        {
            "exp(x+π)*exp(y+τ)",
            make_expr_48,
            "{ exp(x + y + π + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = exp(x + y + π + τ)\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 49 */
        {
            "sin(x+π)*cos(y+τ)",
            make_expr_49,
            "{ sin(x + π)·cos(y + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = sin(x + π)*cos(y + τ)\n"
            "return expr(x,y,τ)",
            __LINE__
        },

        /* 50 */
        {
            "exp(sin(x+π) + cos(y+τ))",
            make_expr_50,
            "{ exp(sin(x + π) + cos(y + τ)) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,τ) = exp(sin(x + π) + cos(y + τ))\n"
            "return expr(x,y,τ)",
            __LINE__
        },
    };

    /* ============================================================
     *  Test loop — formatted with bold PASS/FAIL and file:line
     * ============================================================ */
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();
        dval_t *simp = dv_simplify(f);

        char *got_expr = dv_to_string(simp, style_EXPRESSION);
        char *got_func = dv_to_string(simp, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        /* ---------------- EXPR block ---------------- */
        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        /* ---------------- FUNC block ---------------- */
        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        /* got block */
        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        /* expected block */
        {
            const char *p = expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        free(expected_func_c);
        dv_free(simp);
        dv_free(f);
    }
}

/* ============================================================
 *  Builders for unnamed-variable tests (U01–U06)
 * ============================================================ */

/* U01: x₀² */
dval_t *make_expr_u01(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *f  = dv_mul(x, x);
    dv_free(x);
    return f;
}

/* U02: x₀³ */
dval_t *make_expr_u02(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *t1 = dv_mul(x, x);
    dval_t *f  = dv_mul(t1, x);
    dv_free(x);
    dv_free(t1);
    return f;
}

/* U03: x₀³x₁³  (mirrors test 09, but with unnamed vars) */
dval_t *make_expr_u03(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *y  = test_dv_new_var_d(1.25);
    dval_t *t1 = dv_pow_d(x, 2.0);
    dval_t *t2 = dv_pow_d(y, 3.0);
    dval_t *t3 = dv_mul(t1, t2);
    dval_t *f  = dv_mul(t3, x);
    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return f;
}

/* U04: 2x₀²  (coefficient stays numeric after simplification) */
dval_t *make_expr_u04(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *t1 = dv_mul(x, x);
    dval_t *t2 = dv_mul(x, x);
    dval_t *f  = dv_add(t1, t2);
    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return f;
}

/* U05: sin(x₀)·cos(x₀) */
dval_t *make_expr_u05(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *sx = dv_sin(x);
    dval_t *cx = dv_cos(x);
    dval_t *f  = dv_mul(sx, cx);
    dv_free(x);
    dv_free(sx);
    dv_free(cx);
    return f;
}

/* U06: exp(sin(x₀) + cos(x₀))  (exp merge) */
dval_t *make_expr_u06(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *sx = dv_sin(x);
    dval_t *cx = dv_cos(x);
    dval_t *t1 = dv_exp(sx);
    dval_t *t2 = dv_exp(cx);
    dval_t *f  = dv_mul(t1, t2);
    dv_free(x);
    dv_free(sx);
    dv_free(cx);
    dv_free(t1);
    dv_free(t2);
    return f;
}

/* ============================================================
 *  Builders for manually-subscripted constant tests (C01–C04)
 *
 *  Callers pass "c\xE2\x82\x80" (c₀) and "c\xE2\x82\x81" (c₁)
 *  as the name argument to dv_new_named_const so the names are
 *  simple (letter + subscript digit) — they won't be bracketed.
 * ============================================================ */

/* C01: c₀x₀²  (named const × unnamed var²) */
dval_t *make_expr_c01(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *c  = test_dv_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *f  = dv_mul(c, x2);
    dv_free(x);
    dv_free(c);
    dv_free(x2);
    return f;
}

/* C02: c₀·sin(x₀)  (named const × function — needs separator) */
dval_t *make_expr_c02(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *c  = test_dv_new_named_const_qf(QF_E, "c\xE2\x82\x80");
    dval_t *sx = dv_sin(x);
    dval_t *f  = dv_mul(c, sx);
    dv_free(x);
    dv_free(c);
    dv_free(sx);
    return f;
}

/* C03: x₀ + x₁ + c₀ */
dval_t *make_expr_c03(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *y  = test_dv_new_var_d(1.25);
    dval_t *c  = test_dv_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    dval_t *t1 = dv_add(x, y);
    dval_t *f  = dv_add(t1, c);
    dv_free(x);
    dv_free(y);
    dv_free(c);
    dv_free(t1);
    return f;
}

/* C04: c₀x₀ + c₁  (two named consts with unnamed var; tests multi-const bindings) */
dval_t *make_expr_c04(void)
{
    dval_t *x  = test_dv_new_var_d(1.25);
    dval_t *c0 = test_dv_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    dval_t *c1 = test_dv_new_named_const_qf(QF_E,  "c\xE2\x82\x81");
    dval_t *t1 = dv_mul(c0, x);
    dval_t *f  = dv_add(t1, c1);
    dv_free(x);
    dv_free(c0);
    dv_free(c1);
    dv_free(t1);
    return f;
}

/* ============================================================
 *  Builders for multi-character name tests (L01–L09)
 * ============================================================ */

/* L01: [radius]² */
dval_t *make_expr_l01(void)
{
    dval_t *r = test_dv_new_named_var_d(1.25, "radius");
    dval_t *f = dv_pow_d(r, 2.0);
    dv_free(r);
    return f;
}

/* L02: [base]·[height]  (two multi-char vars — separator needed) */
dval_t *make_expr_l02(void)
{
    dval_t *base   = test_dv_new_named_var_d(1.25, "base");
    dval_t *height = test_dv_new_named_var_d(1.25, "height");
    dval_t *f      = dv_mul(base, height);
    dv_free(base);
    dv_free(height);
    return f;
}

/* L03: [pi]·[radius]²  (multi-char named const × multi-char named var²) */
dval_t *make_expr_l03(void)
{
    dval_t *r  = test_dv_new_named_var_d(1.25, "radius");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "pi");
    dval_t *r2 = dv_pow_d(r, 2.0);
    dval_t *f  = dv_mul(pi, r2);
    dv_free(r);
    dv_free(pi);
    dv_free(r2);
    return f;
}

/* L04: π·[radius]²  (@pi → π is simple; radius is not — separator needed) */
dval_t *make_expr_l04(void)
{
    dval_t *r  = test_dv_new_named_var_d(1.25, "radius");
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *r2 = dv_pow_d(r, 2.0);
    dval_t *f  = dv_mul(pi, r2);
    dv_free(r);
    dv_free(pi);
    dv_free(r2);
    return f;
}

/* L05: sin([theta])·cos([theta]) */
dval_t *make_expr_l05(void)
{
    dval_t *t  = test_dv_new_named_var_d(1.25, "theta");
    dval_t *st = dv_sin(t);
    dval_t *ct = dv_cos(t);
    dval_t *f  = dv_mul(st, ct);
    dv_free(t);
    dv_free(st);
    dv_free(ct);
    return f;
}

/* L06: [pi]·[tau]·x  (two multi-char consts + one single-char var) */
dval_t *make_expr_l06(void)
{
    dval_t *x   = test_dv_new_named_var_d(1.25, "x");
    dval_t *pi  = test_dv_new_named_const_qf(QF_PI,  "pi");
    dval_t *tau = test_dv_new_named_const_qf(QF_2PI, "tau");
    dval_t *t1  = dv_mul(pi, tau);
    dval_t *f   = dv_mul(t1, x);
    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    return f;
}

/* L07: [my var]²  (space in name) */
dval_t *make_expr_l07(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "my var");
    dval_t *f = dv_pow_d(x, 2.0);
    dv_free(x);
    return f;
}

/* L08: [2pi]·x  (name starting with a digit) */
dval_t *make_expr_l08(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x");
    dval_t *c = test_dv_new_named_const_qf(QF_PI, "2pi");
    dval_t *f = dv_mul(c, x);
    dv_free(x);
    dv_free(c);
    return f;
}

/* L09: [x']²  (non-alphanumeric character — apostrophe/prime) */
dval_t *make_expr_l09(void)
{
    dval_t *x = test_dv_new_named_var_d(1.25, "x'");
    dval_t *f = dv_pow_d(x, 2.0);
    dv_free(x);
    return f;
}

/* ============================================================
 *  test_expressions_unnamed
 * ============================================================ */
void test_expressions_unnamed(void)
{
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* U01 */
        {
            "x*x (unnamed)",
            make_expr_u01,
            "{ x₀² | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = x₀^2\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U02 */
        {
            "x*x*x (unnamed)",
            make_expr_u02,
            "{ x₀³ | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = x₀^3\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U03 */
        {
            "x^2*y^3*x (unnamed)",
            make_expr_u03,
            "{ x₀³x₁³ | x₀ = 1.25, x₁ = 1.25 }",
            "x₀ = 1.25\n"
            "x₁ = 1.25\n"
            "expr(x₀,x₁) = x₀^3*x₁^3\n"
            "return expr(x₀,x₁)",
            __LINE__
        },

        /* U04 */
        {
            "x*x + x*x (unnamed)",
            make_expr_u04,
            "{ 2x₀² | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = 2*x₀^2\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U05 */
        {
            "sin(x)*cos(x) (unnamed)",
            make_expr_u05,
            "{ sin(x₀)·cos(x₀) | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = sin(x₀)*cos(x₀)\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U06 */
        {
            "exp(sin(x))*exp(cos(x)) (unnamed)",
            make_expr_u06,
            "{ exp(sin(x₀) + cos(x₀)) | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = exp(sin(x₀) + cos(x₀))\n"
            "return expr(x₀)",
            __LINE__
        },

        /* C01 */
        {
            "c₀*x₀^2 (named const, unnamed var)",
            make_expr_c01,
            "{ c₀x₀² | x₀ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
            "x₀ = 1.25\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "expr(x₀,c₀) = c₀*x₀^2\n"
            "return expr(x₀,c₀)",
            __LINE__
        },

        /* C02 */
        {
            "c₀*sin(x₀) (named const, unnamed var)",
            make_expr_c02,
            "{ c₀·sin(x₀) | x₀ = 1.25; c₀ = 2.718281828459045235360287471352664 }",
            "x₀ = 1.25\n"
            "c₀ = 2.718281828459045235360287471352664\n"
            "expr(x₀,c₀) = c₀*sin(x₀)\n"
            "return expr(x₀,c₀)",
            __LINE__
        },

        /* C03 */
        {
            "x₀ + x₁ + c₀",
            make_expr_c03,
            "{ x₀ + x₁ + c₀ | x₀ = 1.25, x₁ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
            "x₀ = 1.25\n"
            "x₁ = 1.25\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "expr(x₀,x₁,c₀) = x₀ + x₁ + c₀\n"
            "return expr(x₀,x₁,c₀)",
            __LINE__
        },

        /* C04 */
        {
            "c₀*x₀ + c₁ (two named consts, unnamed var)",
            make_expr_c04,
            "{ c₁ + c₀x₀ | x₀ = 1.25; c₁ = 2.718281828459045235360287471352664, c₀ = 3.141592653589793238462643383279505 }",
            "x₀ = 1.25\n"
            "c₁ = 2.718281828459045235360287471352664\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "expr(x₀,c₁,c₀) = c₁ + c₀*x₀\n"
            "return expr(x₀,c₁,c₀)",
            __LINE__
        },
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();
        dval_t *simp = dv_simplify(f);

        char *got_expr = dv_to_string(simp, style_EXPRESSION);
        char *got_func = dv_to_string(simp, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        {
            const char *p = expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        free(expected_func_c);
        dv_free(simp);
        dv_free(f);
    }
}

/* ============================================================
 *  test_expressions_longname
 * ============================================================ */
void test_expressions_longname(void)
{
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* L01 */
        {
            "radius^2",
            make_expr_l01,
            "{ [radius]² | [radius] = 1.25 }",
            "[radius] = 1.25\n"
            "expr([radius]) = [radius]^2\n"
            "return expr([radius])",
            __LINE__
        },

        /* L02 */
        {
            "base * height",
            make_expr_l02,
            "{ [base]·[height] | [base] = 1.25, [height] = 1.25 }",
            "[base] = 1.25\n"
            "[height] = 1.25\n"
            "expr([base],[height]) = [base]*[height]\n"
            "return expr([base],[height])",
            __LINE__
        },

        /* L03 */
        {
            "pi * radius^2",
            make_expr_l03,
            "{ [pi]·[radius]² | [radius] = 1.25 }",
            "[radius] = 1.25\n"
            "expr([radius]) = [pi]*[radius]^2\n"
            "return expr([radius])",
            __LINE__
        },

        /* L04 */
        {
            "@pi * radius^2",
            make_expr_l04,
            "{ π·[radius]² | [radius] = 1.25 }",
            "[radius] = 1.25\n"
            "expr([radius]) = π*[radius]^2\n"
            "return expr([radius])",
            __LINE__
        },

        /* L05 */
        {
            "sin(theta)*cos(theta)",
            make_expr_l05,
            "{ sin([theta])·cos([theta]) | [theta] = 1.25 }",
            "[theta] = 1.25\n"
            "expr([theta]) = sin([theta])*cos([theta])\n"
            "return expr([theta])",
            __LINE__
        },

        /* L06 */
        {
            "pi * tau * x",
            make_expr_l06,
            "{ [pi]·[tau]·x | x = 1.25; [tau] = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "[tau] = 6.283185307179586476925286766559011\n"
            "expr(x,[tau]) = [pi]*[tau]*x\n"
            "return expr(x,[tau])",
            __LINE__
        },

        /* L07: space in name */
        {
            "\"my var\"^2",
            make_expr_l07,
            "{ [my var]² | [my var] = 1.25 }",
            "[my var] = 1.25\n"
            "expr([my var]) = [my var]^2\n"
            "return expr([my var])",
            __LINE__
        },

        /* L08: name starting with a digit */
        {
            "\"2pi\" * x",
            make_expr_l08,
            "{ [2pi]·x | x = 1.25; [2pi] = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "[2pi] = 3.141592653589793238462643383279505\n"
            "expr(x,[2pi]) = [2pi]*x\n"
            "return expr(x,[2pi])",
            __LINE__
        },

        /* L09: non-alphanumeric character (apostrophe/prime) */
        {
            "\"x'\"^2",
            make_expr_l09,
            "{ [x']² | [x'] = 1.25 }",
            "[x'] = 1.25\n"
            "expr([x']) = [x']^2\n"
            "return expr([x'])",
            __LINE__
        },
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();

        char *got_expr = dv_to_string(f, style_EXPRESSION);
        char *got_func = dv_to_string(f, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        {
            const char *p = expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        free(expected_func_c);
        dv_free(f);
    }
}

/* ============================================================
 *  test_dval_t_from_string — dedicated from_string test group
 * ============================================================ */

/* Round-trip helper: build a dval_t, convert to expr string, parse it back,
 * and verify the evaluated value matches the original. */
void check_roundtrip(const char *label, dval_t *f, int line)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    dval_t *g = dval_from_string(s, NULL);

    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (from_string returned NULL) %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string " C_RESET "%s\n\n", s);
        TEST_FAIL();
        free(s);
        dv_free(f);
        return;
    }

    qfloat_t expect = dv_eval_qf(f);
    qfloat_t got    = dv_eval_qf(g);
    qfloat_t diff   = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));
    double rel_err = (exp_d > 0) ? abs_err / exp_d : abs_err;

    const double TOL = 2e-14; /* round-trip through double in binding values */
    if (abs_err < TOL || rel_err < TOL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  string  " C_RESET "%s\n\n", s);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s value mismatch %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        qf_printf(C_BOLD "  got     " C_RESET "%.34q\n", got);
        qf_printf(C_BOLD "  expect  " C_RESET "%.34q\n", expect);
        printf("\n");
        TEST_FAIL();
    }

    free(s);
    dv_free(g);
    dv_free(f);
}

/* Check that parsing an explicit string gives a specific evaluated value. */
void check_parse_val(const char *label, const char *s,
                             double expect_d, int line)
{
    dval_t *g = dval_from_string(s, NULL);
    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (NULL) %s:%d:1\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }
    double got = dv_eval_d(g);
    double err = fabs(got - expect_d);
    double rel = (fabs(expect_d) > 0) ? err / fabs(expect_d) : err;
    const double TOL = 2e-14;
    if (err < TOL || rel < TOL) {
        char *parsed = dv_to_string(g, style_EXPRESSION);
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input   " C_RESET "%s\n", s);
        printf(C_BOLD "  parsed  " C_RESET "%s\n\n", parsed ? parsed : "(null)");
        free(parsed);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        printf(C_BOLD "  got     " C_RESET "%.17g\n", got);
        printf(C_BOLD "  expect  " C_RESET "%.17g\n", expect_d);
        printf("\n");
        TEST_FAIL();
    }
    dv_free(g);
}

/* Check that parsing a string returns NULL (expected error path).
 * Note: dval_from_string prints diagnostics to stderr for error cases. */
void check_parse_null(const char *label, const char *s, int line)
{
    dval_t *g = dval_from_string(s, NULL);
    if (!g) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n\n", label);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (expected NULL) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        dv_free(g);
    }
}

void check_parse_null_stderr_contains(const char *label,
                                      const char *s,
                                      const char *expected_substring,
                                      int line)
{
    const char *capture_path = NULL;
    int saved_stderr;
    dval_t *g;
    FILE *f;
    long size;
    char *buf;
    size_t nread;

    saved_stderr = test_case_begin_stderr_capture("dval-from-string-stderr.txt",
                                                  &capture_path);
    if (saved_stderr < 0 || !capture_path) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture unavailable) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    g = dval_from_string(s, NULL);

    if (!test_case_end_stderr_capture(saved_stderr)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr restore failed) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        if (g)
            dv_free(g);
        return;
    }

    if (g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (expected NULL) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        dv_free(g);
        return;
    }

    f = fopen(capture_path, "rb");
    if (!f) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture unreadable) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture seek failed) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture size failed) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    buf = malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture alloc failed) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    nread = fread(buf, 1, (size_t)size, f);
    buf[nread] = '\0';
    fclose(f);

    if (expected_substring && *expected_substring &&
        !strstr(buf, expected_substring)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (missing stderr substring) %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  expected stderr to contain " C_RESET "%s\n",
               expected_substring);
        printf(C_BOLD "  got stderr             " C_RESET "%s\n\n", buf);
        TEST_FAIL();
        free(buf);
        return;
    }

    printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
    if (expected_substring && *expected_substring)
        printf(C_BOLD "  stderr  " C_RESET "contains \"%s\"\n\n",
               expected_substring);
    else
        printf("\n");
    free(buf);
}

/* ---- Legacy pure-constant parse format: { name = val } ---- */
