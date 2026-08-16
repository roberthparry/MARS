#include "test_expr.h"

typedef struct {
    char *label;
    char *tex;
} TeX_preview_entry_t;

static TeX_preview_entry_t *g_TeX_preview_entries = NULL;
static size_t g_TeX_preview_count = 0u;
static size_t g_TeX_preview_cap = 0u;
static int g_TeX_preview_cleanup_registered = 0;

static void TeX_preview_cleanup(void)
{
    size_t i;

    for (i = 0u; i < g_TeX_preview_count; ++i) {
        free(g_TeX_preview_entries[i].label);
        free(g_TeX_preview_entries[i].tex);
    }
    free(g_TeX_preview_entries);
    g_TeX_preview_entries = NULL;
    g_TeX_preview_count = 0u;
    g_TeX_preview_cap = 0u;
}

static char *TeX_preview_strdup(const char *s)
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

static char *TeX_preview_path_from_source(const char *source_file)
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

static void TeX_preview_write_escaped(FILE *f, const char *s)
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

static void TeX_preview_emit_case(const char *source_file, const char *label, const char *tex)
{
    char *path;
    FILE *f;
    size_t i;

    if (!label || !tex)
        return;

    if (!g_TeX_preview_cleanup_registered) {
        atexit(TeX_preview_cleanup);
        g_TeX_preview_cleanup_registered = 1;
    }

    if (g_TeX_preview_count == g_TeX_preview_cap) {
        size_t new_cap = g_TeX_preview_cap == 0u ? 8u : g_TeX_preview_cap * 2u;
        TeX_preview_entry_t *new_entries = realloc(g_TeX_preview_entries, new_cap * sizeof(*new_entries));
        if (!new_entries)
            return;
        g_TeX_preview_entries = new_entries;
        g_TeX_preview_cap = new_cap;
    }

    g_TeX_preview_entries[g_TeX_preview_count].label = TeX_preview_strdup(label);
    g_TeX_preview_entries[g_TeX_preview_count].tex = TeX_preview_strdup(tex);
    if (!g_TeX_preview_entries[g_TeX_preview_count].label || !g_TeX_preview_entries[g_TeX_preview_count].tex) {
        free(g_TeX_preview_entries[g_TeX_preview_count].label);
        free(g_TeX_preview_entries[g_TeX_preview_count].tex);
        g_TeX_preview_entries[g_TeX_preview_count].label = NULL;
        g_TeX_preview_entries[g_TeX_preview_count].tex = NULL;
        return;
    }
    ++g_TeX_preview_count;

    path = TeX_preview_path_from_source(source_file);
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
    TeX_preview_write_escaped(f, source_file);
    fprintf(f, "}\n\n");

    for (i = 0u; i < g_TeX_preview_count; ++i) {
        fprintf(f, "\\subsection*{Sample %zu}\n", i + 1u);
        fprintf(f, "\\noindent\\texttt{");
        TeX_preview_write_escaped(f, g_TeX_preview_entries[i].label);
        fprintf(f,
                "}\n"
                "\\begin{flushleft}\n"
                "$\\displaystyle %s$\n"
                "\\end{flushleft}\n\n",
                g_TeX_preview_entries[i].tex);
    }

    fprintf(f, "\\end{document}\n");
    fclose(f);
    free(path);
}

/* ------------------------------------------------------------------------- */
/* expr_to_string Tests                                                        */
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

    size_t pos = 0u;
    size_t len = strlen(s);
    int first = 1;

    while (pos < len) {
        size_t line_len;

        if (!first) {
            /* indent continuation lines to same column */
            for (int i = 0; i < base_indent; i++)
                fputc(' ', stderr);
        }

        line_len = strcspn(&s[pos], "\n");
        if (pos + line_len < len) {
            fwrite(&s[pos], 1, line_len + 1u, stderr);
            pos += line_len + 1u;
        } else {
            fprintf(stderr, "%s\n", &s[pos]);
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

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
    TEST_FAIL();
}

/* Compare two strings ignoring trailing whitespace */
int str_eq(const char *a, const char *b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);

    while (la > 0 && (a[la - 1] == '\n' || a[la - 1] == '\r' || a[la - 1] == ' ' || a[la - 1] == '\t'))
        --la;

    while (lb > 0 && (b[lb - 1] == '\n' || b[lb - 1] == '\r' || b[lb - 1] == ' ' || b[lb - 1] == '\t'))
        --lb;

    return la == lb && memcmp(a, b, la) == 0;
}

typedef struct {
    char *buf;
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

static char *test_trim_copy_range(const char *first, const char *last)
{
    size_t start = 0u;
    size_t end = (size_t)(last - first);

    while (start < end && (first[start] == ' ' || first[start] == '\t'))
        start++;
    while (end > start && (first[end - 1u] == ' ' || first[end - 1u] == '\t'))
        end--;
    return test_copy_range(&first[start], &first[end]);
}

static char *test_c_style_body_from_legacy(const char *body)
{
    test_sbuf_t out = {0};

    for (size_t pos = 0u; body[pos] != '\0'; ++pos) {
        if (body[pos] == '*' || body[pos] == '/') {
            if (!test_sbuf_puts(&out, body[pos] == '*' ? "." : "/")) {
                free(out.buf);
                return NULL;
            }
        } else if (!test_sbuf_putc(&out, body[pos])) {
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
    if (strcmp(name, "[pi]") == 0 || strcmp(name, "[tau]") == 0 || strcmp(name, "[2pi]") == 0)
        return 1;
    if (strncmp(name, "c", 1u) == 0 && strstr(name, "\xE2\x82"))
        return 1;

    return 0;
}

static int test_legacy_arg_is_const(const char *name, const test_legacy_binding_t *bindings, size_t nbindings)
{
    for (size_t i = 0u; i < nbindings; ++i) {
        if (bindings[i].is_const && strcmp(bindings[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static int test_emit_c_arg_list(test_sbuf_t *out, const char *args, int typed, const test_legacy_binding_t *bindings,
                                size_t nbindings)
{
    size_t len = args ? strlen(args) : 0u;
    size_t pos = 0u;
    int first = 1;

    if (!args || !*args)
        return test_sbuf_puts(out, typed ? "void" : "");

    while (pos < len) {
        size_t start = pos;
        size_t end;
        char *name;

        while (pos < len && args[pos] != ',')
            pos++;
        end = pos;

        name = test_trim_copy_range(&args[start], &args[end]);
        if (!name)
            return 0;

        if (!first && !test_sbuf_puts(out, ", ")) {
            free(name);
            return 0;
        }
        if (typed && test_legacy_arg_is_const(name, bindings, nbindings) && !test_sbuf_puts(out, "const ")) {
            free(name);
            return 0;
        }
        if (!test_sbuf_puts(out, name)) {
            free(name);
            return 0;
        }

        free(name);
        first = 0;
        if (pos < len && args[pos] == ',')
            pos++;
        while (pos < len && (args[pos] == ' ' || args[pos] == '\t'))
            pos++;
    }

    return 1;
}

static int test_emit_c_variable_bindings(test_sbuf_t *out, const char *args, const test_legacy_binding_t *bindings,
                                         size_t nbindings)
{
    size_t len = args ? strlen(args) : 0u;
    size_t pos = 0u;

    while (pos < len) {
        size_t start = pos;
        size_t end;
        char *name;
        const char *value = "NAN";

        while (pos < len && args[pos] != ',')
            pos++;
        end = pos;
        name = test_trim_copy_range(&args[start], &args[end]);
        if (!name)
            return 0;

        for (size_t i = 0u; i < nbindings; ++i) {
            if (strcmp(bindings[i].name, name) == 0) {
                value = bindings[i].value;
                break;
            }
        }
        if (test_legacy_arg_is_const(name, bindings, nbindings) && !test_sbuf_puts(out, "const ")) {
            free(name);
            return 0;
        }
        if (!test_sbuf_puts(out, name) || !test_sbuf_puts(out, " = ") ||
            !test_sbuf_puts(out, strcmp(value, "NAN") == 0 || strcmp(value, "?") == 0 ? "?" : value) ||
            !test_sbuf_puts(out, ".\n")) {
            free(name);
            return 0;
        }

        free(name);
        if (pos < len && args[pos] == ',')
            pos++;
        while (pos < len && (args[pos] == ' ' || args[pos] == '\t'))
            pos++;
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

    while (line && line[0] != '\0') {
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
                    test_legacy_binding_is_const_name(bindings[nbindings].name, bindings[nbindings].value);
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

    if (!test_sbuf_puts(&out, "expression expr(") || !test_emit_c_arg_list(&out, args, 1, bindings, nbindings) ||
        !test_sbuf_puts(&out, ") {\n") || !test_sbuf_puts(&out, "    return ") || !test_sbuf_puts(&out, body_c) ||
        !test_sbuf_puts(&out, ".\n}\n\n") || !test_emit_c_variable_bindings(&out, args, bindings, nbindings) ||
        !test_sbuf_puts(&out, "output(expr(") || !test_emit_c_arg_list(&out, args, 0, bindings, nbindings) ||
        !test_sbuf_puts(&out, "))."))
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
    expr_t *c = test_expr_new_const_d(3.5);
    char *got = expr_to_string(c, style_EXPRESSION);

    const char *expect = "3.5";

    if (str_eq(got, expect))
        to_string_pass("basic const (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (EXPR)", got, expect);

    free(got);
    expr_free(c);
}

static void test_to_string_basic_const_func(void)
{
    expr_t *c = test_expr_new_const_d(3.5);
    char *got = expr_to_string(c, style_FUNCTION);

    const char *expect = "expression expr() {\n"
                         "    return 3.5.\n"
                         "}\n"
                         "\n"
                         "output(expr()).";

    if (str_eq(got, expect))
        to_string_pass("basic const (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (FUNC)", got, expect);

    free(got);
    expr_free(c);
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
    expr_t *x = test_expr_new_named_var_d(42.0, "x");
    char *got = expr_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 42 }";

    if (str_eq(got, expect))
        to_string_pass("basic var (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (EXPR)", got, expect);

    free(got);
    expr_free(x);
}

static void test_to_string_basic_var_func(void)
{
    expr_t *x = test_expr_new_named_var_d(42.0, "x");
    expr_t *unknown = expr_new_named_var(NUM_NAN, "x");
    char *got = expr_to_string(x, style_FUNCTION);
    char *unknown_got = expr_to_string(unknown, style_FUNCTION);

    const char *expect = "expression expr(x) {\n"
                         "    return x.\n"
                         "}\n"
                         "\n"
                         "x = 42.\n"
                         "output(expr(x)).";
    const char *unknown_expect = "expression expr(x) {\n"
                                 "    return x.\n"
                                 "}\n"
                                 "\n"
                                 "x = ?.\n"
                                 "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("basic var (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (FUNC)", got, expect);

    if (str_eq(unknown_got, unknown_expect))
        to_string_pass("unknown basic var (FUNC)", unknown_got, unknown_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unknown basic var (FUNC)", unknown_got, unknown_expect);

    free(unknown_got);
    free(got);
    expr_free(unknown);
    expr_free(x);
}

void test_to_string_basic_var(void)
{
    TEST_RUN_SUBTEST(test_to_string_basic_var_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var_func, NULL);
}

static void test_to_string_basic_var_TeX(void)
{
    expr_t *x = test_expr_new_named_var_d(42.0, "x0");
    char *got = expr_to_string(x, style_LATEX);

    const char *expect = "\\left\\{ x_{0} \\;\\middle|\\; x_{0} = 42 \\right\\}";

    TeX_preview_emit_case(__FILE__, "basic var (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("basic var (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (TEX)", got, expect);

    free(got);
    expr_free(x);
}

static void test_to_string_nested_transcendental_TeX(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x0");
    expr_t *y = test_expr_new_named_var_d(2.0, "y1");
    expr_t *xy = expr_mul(x, y);
    expr_t *sin_xy = expr_sin(xy);
    expr_t *exp_term = expr_exp(sin_xy);
    expr_t *log_y = expr_log(y);
    expr_t *x_log_y = expr_mul(x, log_y);
    expr_t *f = expr_add(exp_term, x_log_y);
    char *got = expr_to_string(f, style_LATEX);

    const char *expect = "\\left\\{ e^{\\sin(x_{0}\\mkern-2mu y_{1})} + x_{0}\\mkern-2mu \\ln(y_{1}) "
                         "\\;\\middle|\\; x_{0} = 1, y_{1} = 2 \\right\\}";

    TeX_preview_emit_case(__FILE__, "nested transcendental (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("nested transcendental (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested transcendental (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(x_log_y);
    expr_free(log_y);
    expr_free(exp_term);
    expr_free(sin_xy);
    expr_free(xy);
    expr_free(x);
    expr_free(y);
}

static void test_to_string_atan_TeX(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *f = expr_atan(x);
    char *got = expr_to_TeX_body(f);
    const char *expect = "\\arctan(x)";

    if (str_eq(got, expect))
        to_string_pass("arctan (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "arctan (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(x);
}

static void test_to_string_nested_quotient_pow_TeX(void)
{
    expr_t *x = test_expr_new_named_var_d(2.0, "x0");
    expr_t *y = test_expr_new_named_var_d(3.0, "y1");
    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *y2 = expr_pow_d(y, 2.0);
    expr_t *sum = expr_add(x2, y2);
    expr_t *den = expr_add_d(y, 1.0);
    expr_t *frac = expr_div(sum, den);
    expr_t *f = expr_log(frac);
    char *got = expr_to_string(f, style_LATEX);

    const char *expect = "\\left\\{ \\ln(\\frac{x_{0}^{2} + y_{1}^{2}}{y_{1} + 1}) "
                         "\\;\\middle|\\; x_{0} = 2, y_{1} = 3 \\right\\}";

    TeX_preview_emit_case(__FILE__, "nested quotient pow (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("nested quotient pow (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested quotient pow (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(frac);
    expr_free(den);
    expr_free(sum);
    expr_free(y2);
    expr_free(x2);
    expr_free(x);
    expr_free(y);
}

static void test_to_string_log10_TeX(void)
{
    expr_t *x = test_expr_new_named_var_d(100.0, "x0");
    expr_t *f = expr_log10(x);
    char *got = expr_to_string(f, style_LATEX);

    const char *expect = "\\left\\{ \\log(x_{0}) \\;\\middle|\\; x_{0} = 100 \\right\\}";

    TeX_preview_emit_case(__FILE__, "log10 (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("log10 (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log10 (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(x);
}

static void test_to_string_exp_unit_fraction_root_TeX(void)
{
    expr_t *eighth = expr_new_const(NUM_ONE_EIGHTH);
    expr_t *f = expr_exp(eighth);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;
    const char *expect = "\\sqrt[8]{e}";

    if (str_eq(got, expect))
        to_string_pass("exp unit fraction renders as TeX root", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp unit fraction renders as TeX root", got ? got : "(null)", expect);

    free(got);
    expr_free(f);
    expr_free(eighth);
}

static void test_to_string_parsed_exp_unit_fraction_root_TeX(void)
{
    expr_t *f = expr_from_string("{ exp(1/8) }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;
    const char *expect = "\\sqrt[8]{e}";

    if (str_eq(got, expect))
        to_string_pass("parsed exp unit fraction renders as TeX root", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "parsed exp unit fraction renders as TeX root", got ? got : "(null)",
                       expect);

    free(got);
    expr_free(f);
}

static void test_to_string_symbolic_constants_TeX(void)
{
    expr_t *f = expr_from_string("{ exp(@pi*i*3/2*x) }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;

    const char *expect = "\\left\\{ e^{\\pi\\mkern-2mu i\\mkern-2mu \\frac{3}{2}\\mkern-2mu x} "
                         "\\;\\middle|\\; x = NAN \\right\\}";

    TeX_preview_emit_case(__FILE__, "symbolic constants (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("symbolic constants (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "symbolic constants (TEX)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_symbolic_constant_quotient_TeX(void)
{
    expr_t *f = expr_from_string("{ 1/pi }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;

    const char *expect = "\\frac{1}{\\pi}";

    TeX_preview_emit_case(__FILE__, "symbolic constant quotient (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("symbolic constant quotient (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "symbolic constant quotient (TEX)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_lambert_w_TeX(void)
{
    expr_t *x0 = test_expr_new_named_var_d(1.0, "x0");
    expr_t *x1 = test_expr_new_named_var_s("-0.2", "x1");
    expr_t *w0 = expr_lambert_w0(x0);
    expr_t *wm1 = expr_lambert_wm1(x1);
    char *got_w0 = expr_to_string(w0, style_LATEX);
    char *got_wm1 = expr_to_string(wm1, style_LATEX);

    const char *expect_w0 = "\\left\\{ W_{0}(x_{0}) \\;\\middle|\\; x_{0} = 1 \\right\\}";
    const char *expect_wm1 = "\\left\\{ W_{-1}(x_{1}) \\;\\middle|\\; x_{1} = -0.2 \\right\\}";

    TeX_preview_emit_case(__FILE__, "lambert W0 (TEX)", got_w0);
    TeX_preview_emit_case(__FILE__, "lambert W-1 (TEX)", got_wm1);

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
    expr_free(w0);
    expr_free(wm1);
    expr_free(x0);
    expr_free(x1);
}

static void test_to_string_gammainv_TeX(void)
{
    expr_t *f = expr_from_string("{ lgamma(x) - ln(5) | x = gammainv(5) }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;

    const char *expect = "\\left\\{ \\log\\Gamma(x) - \\ln(5) \\;\\middle|\\; "
                         "x = \\Gamma^{-1}(5) \\right\\}";

    TeX_preview_emit_case(__FILE__, "gammainv inverse gamma (TEX)", got);

    if (str_eq(got, expect))
        to_string_pass("gammainv inverse gamma (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "gammainv inverse gamma (TEX)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_gamma_polygamma_standard_names(void)
{
    expr_t *f = expr_from_string("{ gamma(x) + digamma(x) + trigamma(x) + polygamma(2, x) | x = 3 }", NULL);
    expr_t *second = expr_from_string("{ gamma(x)*(trigamma(x)+digamma(x)^2) | x = 2 }", NULL);
    char *got_expr = f ? expr_to_string(f, style_EXPRESSION) : NULL;
    char *got_TeX = f ? expr_to_string(f, style_LATEX) : NULL;
    char *got_func = f ? expr_to_string(f, style_FUNCTION) : NULL;
    char *got_second_TeX = second ? expr_to_string(second, style_LATEX) : NULL;

    const char *expect_expr = "{ Γ(x) + ψ⁽⁰⁾(x) + ψ⁽¹⁾(x) + ψ⁽²⁾(x) | x = 3 }";
    const char *expect_TeX = "\\left\\{ \\Gamma(x) + \\psi^{(0)}(x) + \\psi^{(1)}(x) + \\psi^{(2)}(x) "
                             "\\;\\middle|\\; x = 3 \\right\\}";
    const char *expect_func = "expression expr(x) {\n"
                              "    return gamma(x) + digamma(x) + trigamma(x) + polygamma(2, x).\n"
                              "}\n"
                              "\n"
                              "x = 3.\n"
                              "output(expr(x)).";
    const char *expect_second_TeX = "\\left\\{ \\Gamma(x)\\mkern-2mu \\left(\\psi^{(1)}(x) + "
                                    "\\psi^{(0)}(x)^{2}\\right) \\;\\middle|\\; x = 2 \\right\\}";

    if (str_eq(got_expr, expect_expr))
        to_string_pass("gamma/polygamma standard names (EXPR)", got_expr, expect_expr);
    else
        to_string_fail(__FILE__, __LINE__, 1, "gamma/polygamma standard names (EXPR)", got_expr, expect_expr);

    if (str_eq(got_TeX, expect_TeX))
        to_string_pass("gamma/polygamma standard names (TEX)", got_TeX, expect_TeX);
    else
        to_string_fail(__FILE__, __LINE__, 1, "gamma/polygamma standard names (TEX)", got_TeX, expect_TeX);

    if (str_eq(got_func, expect_func))
        to_string_pass("gamma/polygamma standard names (FUNCTION)", got_func, expect_func);
    else
        to_string_fail(__FILE__, __LINE__, 1, "gamma/polygamma standard names (FUNCTION)", got_func, expect_func);

    if (str_eq(got_second_TeX, expect_second_TeX))
        to_string_pass("gamma second derivative polygamma power (TEX)", got_second_TeX, expect_second_TeX);
    else
        to_string_fail(__FILE__, __LINE__, 1, "gamma second derivative polygamma power (TEX)", got_second_TeX,
                       expect_second_TeX);

    free(got_expr);
    free(got_TeX);
    free(got_func);
    free(got_second_TeX);
    expr_free(f);
    expr_free(second);
}

static void test_to_string_non_simple_var_bracketed_expr(void)
{
    expr_t *v = test_expr_new_named_var_d(42.0, "a0b0");
    char *got = expr_to_string(v, style_EXPRESSION);

    const char *expect = "{ [a0b₀] | [a0b₀] = 42 }";

    if (str_eq(got, expect))
        to_string_pass("non-simple var bracketed (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "non-simple var bracketed (EXPR)", got, expect);

    free(got);
    expr_free(v);
}

static void test_to_string_non_simple_var_bracketed_func(void)
{
    expr_t *v = test_expr_new_named_var_d(42.0, "a0b0");
    char *got = expr_to_string(v, style_FUNCTION);

    const char *expect = "expression expr([a0b₀]) {\n"
                         "    return [a0b₀].\n"
                         "}\n"
                         "\n"
                         "[a0b₀] = 42.\n"
                         "output(expr([a0b₀])).";

    if (str_eq(got, expect))
        to_string_pass("non-simple var bracketed (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "non-simple var bracketed (FUNC)", got, expect);

    free(got);
    expr_free(v);
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
    expr_t *x = test_expr_new_named_var_d(1, "x");
    expr_t *y = test_expr_new_named_var_d(2, "y");
    expr_t *f = expr_add(x, y);

    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 1, y = 2 }";

    if (str_eq(got, expect))
        to_string_pass("addition (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (EXPR)", got, expect);

    free(got);
    expr_free(x);
    expr_free(y);
    expr_free(f);
}

static void test_to_string_addition_func(void)
{
    expr_t *x = test_expr_new_named_var_d(1, "x");
    expr_t *y = test_expr_new_named_var_d(2, "y");
    expr_t *f = expr_add(x, y);

    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x, y) {\n"
                         "    return x + y.\n"
                         "}\n"
                         "\n"
                         "x = 1.\n"
                         "y = 2.\n"
                         "output(expr(x, y)).";

    if (str_eq(got, expect))
        to_string_pass("addition (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (FUNC)", got, expect);

    free(got);
    expr_free(x);
    expr_free(y);
    expr_free(f);
}

void test_to_string_addition(void)
{
    TEST_RUN_SUBTEST(test_to_string_addition_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_addition_func, NULL);
}

static void test_to_string_wrapped_TeX_aligned_subtraction(void)
{
    expr_t *x = test_expr_new_named_var_d(1, "x");
    expr_t *y = test_expr_new_named_var_d(2, "y");
    expr_t *z = test_expr_new_named_var_d(3, "z");
    expr_t *neg_y = expr_neg(y);
    expr_t *x_minus_y = expr_add(x, neg_y);
    expr_t *f = expr_add(x_minus_y, z);
    char *got = expr_to_TeX_body_wrapped(f, 1u);
    const char *expect = "aligned wrapped TeX with subtraction, not + -";

    if (got && strstr(got, "\\begin{aligned}[t]") && strstr(got, "\\\\") && strstr(got, "{} - y") &&
        !strstr(got, "+ -"))
        to_string_pass("wrapped TeX aligned subtraction", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "wrapped TeX aligned subtraction", got, expect);

    free(got);
    expr_free(f);
    expr_free(x_minus_y);
    expr_free(neg_y);
    expr_free(x);
    expr_free(y);
    expr_free(z);
}

static void test_to_string_wrapped_TeX_distributes_scale(void)
{
    expr_t *x = test_expr_new_named_var_d(1, "x");
    expr_t *y = test_expr_new_named_var_d(2, "y");
    expr_t *z = test_expr_new_named_var_d(3, "z");
    expr_t *k = test_expr_new_named_var_d(4, "k");
    expr_t *neg_y = expr_neg(y);
    expr_t *x_minus_y = expr_add(x, neg_y);
    expr_t *sum = expr_add(x_minus_y, z);
    expr_t *f = expr_mul(k, sum);
    char *got = expr_to_TeX_body_wrapped(f, 1u);
    const char *expect = "scaled wrapped TeX distributes factor without tall delimiters";

    if (got && strstr(got, "\\begin{aligned}[t]") && strstr(got, "k\\mkern-2mu x") &&
        strstr(got, "{} - k\\mkern-2mu y") && strstr(got, "{} + k\\mkern-2mu z") &&
        !strstr(got, "\\left(\\begin{aligned}"))
        to_string_pass("wrapped TeX distributes scale", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "wrapped TeX distributes scale", got, expect);

    free(got);
    expr_free(f);
    expr_free(sum);
    expr_free(x_minus_y);
    expr_free(neg_y);
    expr_free(x);
    expr_free(y);
    expr_free(z);
    expr_free(k);
}

static void test_to_string_negative_rhs_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");
    expr_t *z = test_expr_new_named_var_d(4, "z");
    expr_t *neg_y = expr_neg(y);
    expr_t *frac = expr_div(neg_y, z);
    expr_t *f = expr_sub(x, frac);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y/z | x = 2, y = 3, z = 4 }";

    if (str_eq(got, expect))
        to_string_pass("negative rhs quotient (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative rhs quotient (EXPR)", got, expect);

    free(got);
    expr_free(neg_y);
    expr_free(frac);
    expr_free(f);
    expr_free(x);
    expr_free(y);
    expr_free(z);
}

static void test_to_string_double_negative_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");
    expr_t *neg_y = expr_neg(y);
    expr_t *f = expr_sub(x, neg_y);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("double negative rhs (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "double negative rhs (EXPR)", got, expect);

    free(got);
    expr_free(neg_y);
    expr_free(f);
    expr_free(x);
    expr_free(y);
}

static void test_to_string_nested_negative_rhs_expr(void)
{
    expr_t *a = test_expr_new_named_var_d(5, "a");
    expr_t *b = test_expr_new_named_var_d(6, "b");
    expr_t *one = test_expr_new_const_d(1);
    expr_t *two = test_expr_new_const_d(2);
    expr_t *two_over_a = expr_div(two, a);
    expr_t *inner = expr_sub(one, two_over_a);
    expr_t *neg_inner = expr_neg(inner);
    expr_t *neg_inner_over_a = expr_div(neg_inner, a);
    expr_t *minus_two = test_expr_new_const_d(-2);
    expr_t *rhs = expr_div(neg_inner_over_a, b);
    expr_t *lhs = expr_div(minus_two, a);
    expr_t *f = expr_sub(lhs, rhs);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ -2/a + (1 - 2/a)/a/b | a = 5, b = 6 }";

    if (str_eq(got, expect))
        to_string_pass("nested negative rhs quotient (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested negative rhs quotient (EXPR)", got, expect);

    free(got);
    expr_free(f);
    expr_free(lhs);
    expr_free(rhs);
    expr_free(minus_two);
    expr_free(neg_inner_over_a);
    expr_free(neg_inner);
    expr_free(inner);
    expr_free(two_over_a);
    expr_free(two);
    expr_free(one);
    expr_free(a);
    expr_free(b);
}

/* ============================================================
 * NESTED MUL + ADD
 * ============================================================ */

static void test_to_string_nested_mul_add_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");
    expr_t *z = test_expr_new_named_var_d(4, "z");

    expr_t *xy = expr_mul(x, y);
    expr_t *f = expr_add(xy, z);
    expr_t *simp = expr_simplify(f);

    char *got = expr_to_string(simp, style_EXPRESSION);
    const char *expect = "{ xy + z | z = 4, x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (EXPR)", got, expect);

    free(got);
    expr_free(simp);
    expr_free(xy);
    expr_free(x);
    expr_free(y);
    expr_free(z);
    expr_free(f);
}

static void test_to_string_nested_mul_add_func(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");
    expr_t *z = test_expr_new_named_var_d(4, "z");

    expr_t *xy = expr_mul(x, y);
    expr_t *f = expr_add(xy, z);
    expr_t *simp = expr_simplify(f);

    char *got = expr_to_string(simp, style_FUNCTION);
    const char *expect = "expression expr(z, x, y) {\n"
                         "    return z + x.y.\n"
                         "}\n"
                         "\n"
                         "z = 4.\n"
                         "x = 2.\n"
                         "y = 3.\n"
                         "output(expr(z, x, y)).";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (FUNC)", got, expect);

    free(got);
    expr_free(simp);
    expr_free(xy);
    expr_free(x);
    expr_free(y);
    expr_free(z);
    expr_free(f);
}

static void test_to_string_polynomial_degree_order_expr(void)
{
    struct {
        const char *label;
        const char *source;
        const char *expected;
    } cases[] = {{"polynomial terms sort by degree in x then y", "{ y^2+x*y+x^2 | x = NAN, y = NAN }",
                  "{ x² + xy + y² | y = NAN, x = NAN }"},
                 {"polynomial terms sort lexicographically across variables",
                  "{ c*y^3+b*x*y^2+a*x*y+x^2 | x = NAN, y = NAN; a = NAN, b = NAN, c = NAN }",
                  "{ x² + bxy² + axy + cy³ | y = NAN, x = NAN; c = NAN, b = NAN, a = NAN }"},
                 {"polynomial terms sort by x then y then z", "{ y*z^2+x*z+x*y^2+x^2 | x = NAN, y = NAN, z = NAN }",
                  "{ x² + xy² + xz + yz² | y = NAN, z = NAN, x = NAN }"}};

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_t *f = expr_from_string(cases[i].source, NULL);
        char *got = f ? expr_to_string(f, style_EXPRESSION) : NULL;

        if (got && str_eq(got, cases[i].expected))
            to_string_pass(cases[i].label, got, cases[i].expected);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, got, cases[i].expected);

        free(got);
        expr_free(f);
    }
}

void test_to_string_nested_mul_add(void)
{
    TEST_RUN_SUBTEST(test_to_string_nested_mul_add_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_mul_add_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_polynomial_degree_order_expr, NULL);
}

static void test_to_string_atan2_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");

    expr_t *f = expr_atan2(x, y);

    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ atan2(x, y) | x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("atan2 (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (EXPR)", got, expect);

    free(got);
    expr_free(x);
    expr_free(y);
    expr_free(f);
}

static void test_to_string_atan2_func(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *y = test_expr_new_named_var_d(3, "y");

    expr_t *f = expr_atan2(x, y);

    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x, y) {\n"
                         "    return atan2(x, y).\n"
                         "}\n"
                         "\n"
                         "x = 2.\n"
                         "y = 3.\n"
                         "output(expr(x, y)).";

    if (str_eq(got, expect))
        to_string_pass("atan2 (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (FUNC)", got, expect);

    free(got);
    expr_free(x);
    expr_free(y);
    expr_free(f);
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
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *f = expr_pow_d(x, 3);

    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ x³ | x = 2 }";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (EXPR)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
}

static void test_to_string_pow_superscript_func(void)
{
    expr_t *x = test_expr_new_named_var_d(2, "x");
    expr_t *f = expr_pow_d(x, 3);

    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x) {\n"
                         "    return x^3.\n"
                         "}\n"
                         "\n"
                         "x = 2.\n"
                         "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (FUNC)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
}

static void test_to_string_complex_const_pow_expr(void)
{
    number_t n = num_create_from_string("1 + 2i");
    expr_t *base = expr_new_const(n);
    expr_t *pow = expr_pow_d(base, 6.0);
    expr_t *f = expr_add_d(pow, 1.0);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "(1 + 2i)⁶ + 1";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (EXPR)", got, expect);

    free(got);
    expr_free(f);
    expr_free(pow);
    expr_free(base);
    num_destroy(&n);
}

static void test_to_string_complex_const_pow_func(void)
{
    number_t n = num_create_from_string("1 + 2i");
    expr_t *base = expr_new_const(n);
    expr_t *pow = expr_pow_d(base, 6.0);
    expr_t *f = expr_add_d(pow, 1.0);
    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr() {\n"
                         "    return (1 + 2i)^6 + 1.\n"
                         "}\n"
                         "\n"
                         "output(expr()).";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (FUNC)", got, expect);

    free(got);
    expr_free(f);
    expr_free(pow);
    expr_free(base);
    num_destroy(&n);
}

static void test_to_string_complex_const_pow_TeX(void)
{
    number_t n = num_create_from_string("1 + 2i");
    expr_t *base = expr_new_const(n);
    expr_t *pow = expr_pow_d(base, 6.0);
    expr_t *f = expr_add_d(pow, 1.0);
    char *got = expr_to_string(f, style_LATEX);
    const char *expect = "\\left(1 + 2i\\right)^{6} + 1";

    if (str_eq(got, expect))
        to_string_pass("complex const power base (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex const power base (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(pow);
    expr_free(base);
    num_destroy(&n);
}

static void test_to_string_parsed_complex_const_pow_TeX(void)
{
    expr_t *f = expr_from_string("{ (1 + 2i)^6 + 1 }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;
    const char *expect = "\\left(1 + 2i\\right)^{6} + 1";

    if (str_eq(got, expect))
        to_string_pass("parsed complex const power base (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "parsed complex const power base (TEX)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_parsed_complex_const_pow_expr(void)
{
    expr_t *f = expr_from_string("{ (1 + 2i)^6 + 1 }", NULL);
    char *got = f ? expr_to_string(f, style_EXPRESSION) : NULL;
    const char *expect = "(1 + 2i)⁶ + 1";

    if (str_eq(got, expect))
        to_string_pass("parsed complex const power base (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "parsed complex const power base (EXPR)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_power_base_is_grouped_TeX(void)
{
    expr_t *a = test_expr_new_named_var_d(2, "a");
    expr_t *x = test_expr_new_named_var_d(3, "x");
    expr_t *neg_x = expr_neg(x);
    expr_t *inner = expr_pow_xp(a, neg_x);
    expr_t *f = expr_pow(inner, &NUM_TWO);
    char *got = expr_to_string(f, style_LATEX);
    const char *expect = "\\left\\{ \\left(a^{-x}\\right)^{2} \\;\\middle|\\; "
                         "a = 2, x = 3 \\right\\}";

    if (str_eq(got, expect))
        to_string_pass("power base is grouped (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "power base is grouped (TEX)", got, expect);

    free(got);
    expr_free(f);
    expr_free(inner);
    expr_free(neg_x);
    expr_free(x);
    expr_free(a);
}

static void test_to_string_negative_const_power_base_is_grouped(void)
{
    expr_t *f = expr_from_string("{ (-1)^k | k = NAN }", NULL);
    char *expression = f ? expr_to_string(f, style_EXPRESSION) : NULL;
    char *function = f ? expr_to_string(f, style_FUNCTION) : NULL;
    const char *expected = "(-1)^k";

    if (expression && function && strstr(expression, expected) && strstr(function, expected))
        to_string_pass("negative constant power base is grouped", expression, expected);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative constant power base is grouped",
                       expression ? expression : function, expected);

    free(function);
    free(expression);
    expr_free(f);
}

static void test_to_string_power_of_power_simplifies_expr(void)
{
    expr_t *f = expr_from_string("{ (a^(-x))² | x = NAN; a = NAN }", NULL);
    expr_t *simp = f ? expr_simplify(f) : NULL;
    char *got = simp ? expr_to_string(simp, style_EXPRESSION) : NULL;
    const char *expect = "{ a^(-2x) | x = NAN; a = NAN }";

    if (str_eq(got, expect))
        to_string_pass("power of power simplifies (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "power of power simplifies (EXPR)", got, expect);

    free(got);
    expr_free(simp);
    expr_free(f);
}

static void test_to_string_powered_exponent_TeX(void)
{
    expr_t *f = expr_from_string("{ a^(x^2) | x = NAN; a = NAN }", NULL);
    char *got = f ? expr_to_string(f, style_LATEX) : NULL;
    const char *expect = "\\left\\{ a^{x^{2}} \\;\\middle|\\; "
                         "x = NAN; a = NAN \\right\\}";

    if (str_eq(got, expect))
        to_string_pass("powered exponent renders without double superscript (TEX)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "powered exponent renders without double superscript (TEX)", got, expect);

    free(got);
    expr_free(f);
}

void test_to_string_pow_superscript(void)
{
    TEST_RUN_SUBTEST(test_to_string_pow_superscript_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_pow_superscript_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_complex_const_pow_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_parsed_complex_const_pow_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_parsed_complex_const_pow_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_power_base_is_grouped_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_negative_const_power_base_is_grouped, NULL);
    TEST_RUN_SUBTEST(test_to_string_power_of_power_simplifies_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_powered_exponent_TeX, NULL);
}

/* ============================================================
 * UNARY SIN
 * ============================================================ */

static void test_to_string_unary_sin_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(0.5, "x");
    expr_t *f = expr_sin(x);

    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ sin(x) | x = 0.5 }";

    if (str_eq(got, expect))
        to_string_pass("unary sin (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (EXPR)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
}

static void test_to_string_unary_sin_func(void)
{
    expr_t *x = test_expr_new_named_var_d(0.5, "x");
    expr_t *f = expr_sin(x);

    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x) {\n"
                         "    return sin(x).\n"
                         "}\n"
                         "\n"
                         "x = 0.5.\n"
                         "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("unary sin (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (FUNC)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
}

void test_to_string_unary_sin(void)
{
    TEST_RUN_SUBTEST(test_to_string_unary_sin_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_unary_sin_func, NULL);
}

static void test_to_string_unary_sqrt_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(4.0, "x");
    expr_t *f = expr_sqrt(x);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ √(x) | x = 4 }";

    if (str_eq(got, expect))
        to_string_pass("unary sqrt (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sqrt (EXPR)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
}

static void test_to_string_unary_sqrt_func(void)
{
    expr_t *x = test_expr_new_named_var_d(4.0, "x");
    expr_t *f = expr_sqrt(x);
    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x) {\n"
                         "    return sqrt(x).\n"
                         "}\n"
                         "\n"
                         "x = 4.\n"
                         "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("unary sqrt (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sqrt (FUNC)", got, expect);

    free(got);
    expr_free(x);
    expr_free(f);
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
    expr_t *x = test_expr_new_named_var_d(10, "x");
    char *got = expr_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 10 }";

    if (str_eq(got, expect))
        to_string_pass("function style identity (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (EXPR)", got, expect);

    free(got);
    expr_free(x);
}

static void test_to_string_function_style_func(void)
{
    expr_t *x = test_expr_new_named_var_d(10, "x");
    char *got = expr_to_string(x, style_FUNCTION);

    const char *expect = "expression expr(x) {\n"
                         "    return x.\n"
                         "}\n"
                         "\n"
                         "x = 10.\n"
                         "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("function style identity (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (FUNC)", got, expect);

    free(got);
    expr_free(x);
}

static void test_to_string_function_style_signed_sum(void)
{
    expr_t *x = test_expr_new_named_var_d(1, "x");
    expr_t *three = test_expr_new_const_d(3);
    expr_t *minus_seven = test_expr_new_const_d(-7);
    expr_t *sin_x = expr_sin(x);
    expr_t *exp_sin_x = expr_exp(sin_x);
    expr_t *x2 = expr_mul(x, x);
    expr_t *three_x2 = expr_mul(three, x2);
    expr_t *sum = expr_add(exp_sin_x, three_x2);
    expr_t *f = expr_add(sum, minus_seven);
    expr_t *simp = expr_simplify(f);
    char *got = expr_to_string(simp, style_FUNCTION);

    const char *expect = "expression expr(x) {\n"
                         "    return exp(sin(x)) + 3.x^2 - 7.\n"
                         "}\n"
                         "\n"
                         "x = 1.\n"
                         "output(expr(x)).";

    if (str_eq(got, expect))
        to_string_pass("function style signed sum (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style signed sum (FUNC)", got, expect);

    free(got);
    expr_free(simp);
    expr_free(x);
    expr_free(three);
    expr_free(minus_seven);
    expr_free(sin_x);
    expr_free(exp_sin_x);
    expr_free(x2);
    expr_free(three_x2);
    expr_free(sum);
    expr_free(f);
}

static void test_to_string_function_style_sub_negative_product(void)
{
    expr_t *E = test_expr_new_named_var_d(0.8, "E");
    expr_t *ecc = test_expr_new_named_var_d(0.0167, "e");
    expr_t *sin_E = expr_sin(E);
    expr_t *ecc_sin_E = expr_mul(ecc, sin_E);
    expr_t *f = expr_sub(E, ecc_sin_E);
    char *got = expr_to_string(f, style_FUNCTION);

    const char *expect = "expression expr(E, e) {\n"
                         "    return E - e.sin(E).\n"
                         "}\n"
                         "\n"
                         "E = 0.8000000000000000444089209850062616.\n"
                         "e = 0.01669999999999999956701302039618894.\n"
                         "output(expr(E, e)).";

    if (str_eq(got, expect))
        to_string_pass("function style negative product rhs (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style negative product rhs (FUNC)", got, expect);

    free(got);
    expr_free(f);
    expr_free(ecc_sin_E);
    expr_free(sin_E);
    expr_free(ecc);
    expr_free(E);
}

static void test_to_string_function_style_preserves_math_names(void)
{
    /* README example: style_FUNCTION variable and constant declarations. */
    expr_t *f = expr_from_string(
        "{ tan(x*y*c0/2) | "
        "x = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165, "
        "y = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165; c0 = gamma }",
        NULL);
    char *got = expr_to_string(f, style_FUNCTION);

    const char *expect = "expression expr(x, y, const c₀) {\n"
                         "    return tan(c₀.x.y/2).\n"
                         "}\n"
                         "\n"
                         "x = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165.\n"
                         "y = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165.\n"
                         "const c₀ = @gamma.\n"
                         "output(expr(x, y, c₀)).";

    if (str_eq(got, expect))
        to_string_pass("function style preserves mathematical names (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style preserves mathematical names (FUNC)", got, expect);

    free(got);
    expr_free(f);
}

static void test_to_string_function_style_extracts_variable_dependent_dag_nodes(void)
{
    expr_t *f = expr_from_string("cos(sin(x))+exp(sin(x))+ln(sin(x))+tan(sin(x))+sqrt(sin(x))+asin(sin(x))"
                                 "+cosh(sin(x))+sinh(sin(x))+tanh(sin(x))+acos(sin(x))+atan(sin(x))",
                                 NULL);
    char *got = expr_to_string(f, style_FUNCTION);
    const char *expected_assignment = "    v1 = sin(x).\n";

    if (got && !strstr(got, "Intermediate expressions") && strstr(got, expected_assignment) &&
        !strstr(got, "const v1 = sin(x)."))
        to_string_pass("function style extracts variable-dependent DAG nodes (FUNC)", got, expected_assignment);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style extracts variable-dependent DAG nodes (FUNC)", got,
                       expected_assignment);

    free(got);
    expr_free(f);
}

static void test_to_string_function_style_extracts_short_shared_dag_nodes(void)
{
    expr_t *f = expr_from_string("cos(sin(x))+exp(sin(x))", NULL);
    char *got = expr_to_string(f, style_FUNCTION);
    const char *expected_assignment = "    v1 = sin(x).\n";
    const char *expected_return = "    return cos(v1) + exp(v1).\n";

    if (got && !strstr(got, "Intermediate expressions") && strstr(got, expected_assignment) &&
        strstr(got, expected_return))
        to_string_pass("function style extracts short shared DAG nodes (FUNC)", got, expected_return);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style extracts short shared DAG nodes (FUNC)", got,
                       expected_return);

    free(got);
    expr_free(f);
}

void test_to_string_function_style(void)
{
    TEST_RUN_SUBTEST(test_to_string_function_style_expr, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_func, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_signed_sum, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_sub_negative_product, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_preserves_math_names, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_extracts_variable_dependent_dag_nodes, NULL);
    TEST_RUN_SUBTEST(test_to_string_function_style_extracts_short_shared_dag_nodes, NULL);
}

static void test_to_string_floor_ceil_expr(void)
{
    expr_t *x = test_expr_new_named_var_d(1.5, "x");
    expr_t *y = test_expr_new_named_var_d(-1.5, "y");
    expr_t *floor_x = expr_floor(x);
    expr_t *ceil_y = expr_ceil(y);
    expr_t *f = expr_add(floor_x, ceil_y);
    char *got = expr_to_string(f, style_EXPRESSION);
    const char *expect = "{ ⌊x⌋ + ⌈y⌉ | x = 1.5, y = -1.5 }";

    if (str_eq(got, expect))
        to_string_pass("floor/ceil mathematical notation (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "floor/ceil mathematical notation (EXPR)", got, expect);

    free(got);
    expr_free(f);
    expr_free(ceil_y);
    expr_free(floor_x);
    expr_free(y);
    expr_free(x);
}

static void test_to_string_floor_ceil_func(void)
{
    expr_t *x = test_expr_new_named_var_d(1.5, "x");
    expr_t *y = test_expr_new_named_var_d(-1.5, "y");
    expr_t *floor_x = expr_floor(x);
    expr_t *ceil_y = expr_ceil(y);
    expr_t *f = expr_add(floor_x, ceil_y);
    char *got = expr_to_string(f, style_FUNCTION);
    const char *expect = "expression expr(x, y) {\n"
                         "    return floor(x) + ceil(y).\n"
                         "}\n"
                         "\n"
                         "x = 1.5.\n"
                         "y = -1.5.\n"
                         "output(expr(x, y)).";

    if (str_eq(got, expect))
        to_string_pass("floor/ceil function notation (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "floor/ceil function notation (FUNC)", got, expect);

    free(got);
    expr_free(f);
    expr_free(ceil_y);
    expr_free(floor_x);
    expr_free(y);
    expr_free(x);
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
void check_roundtrip(const char *label, expr_t *f, int line);

void test_to_string_special_functions(void)
{
    /* Unary functions */
    {
        expr_t *x = test_expr_new_named_var_d(-3.0, "x");
        check_roundtrip("to_string: abs(x)", expr_abs(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.5, "x");
        check_roundtrip("to_string: floor(x)", expr_floor(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.5, "x");
        check_roundtrip("to_string: ceil(x)", expr_ceil(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.5, "x");
        check_roundtrip("to_string: erf(x)", expr_erf(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.5, "x");
        check_roundtrip("to_string: erfc(x)", expr_erfc(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.5, "x");
        check_roundtrip("to_string: erfinv(x)", expr_erfinv(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.5, "x");
        check_roundtrip("to_string: erfcinv(x)", expr_erfcinv(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        check_roundtrip("to_string: gamma(x)", expr_gamma(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.329340388179137, "x");
        check_roundtrip("to_string: gammainv(x)", expr_gammainv(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        check_roundtrip("to_string: lgamma(x)", expr_lgamma(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.0, "x");
        check_roundtrip("to_string: digamma(x)", expr_digamma(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.0, "x");
        check_roundtrip("to_string: W₀(x)", expr_lambert_w0(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(-0.2, "x");
        check_roundtrip("to_string: W₋₁(x)", expr_lambert_wm1(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.0, "x");
        check_roundtrip("to_string: normal_pdf(x)", expr_normal_pdf(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.0, "x");
        check_roundtrip("to_string: normal_cdf(x)", expr_normal_cdf(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(0.0, "x");
        check_roundtrip("to_string: normal_logpdf(x)", expr_normal_logpdf(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.0, "x");
        check_roundtrip("to_string: ei(x)", expr_ei(x), __LINE__);
        expr_free(x);
    }
    {
        expr_t *x = test_expr_new_named_var_d(1.0, "x");
        check_roundtrip("to_string: e1(x)", expr_e1(x), __LINE__);
        expr_free(x);
    }
    /* Binary functions */
    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(3.0, "y");
        expr_t *f = expr_beta(x, y);
        expr_free(x);
        expr_free(y);
        check_roundtrip("to_string: beta(x,y)", f, __LINE__);
    }
    {
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *y = test_expr_new_named_var_d(3.0, "y");
        expr_t *f = expr_logbeta(x, y);
        expr_free(x);
        expr_free(y);
        check_roundtrip("to_string: logbeta(x,y)", f, __LINE__);
    }
    {
        expr_t *order = test_expr_new_named_const_d(0.5, "nu");
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *f = expr_bessel_j(order, x);
        expr_free(order);
        expr_free(x);
        check_roundtrip("to_string: BesselJ(nu,x)", f, __LINE__);
    }
    {
        expr_t *order = test_expr_new_named_const_d(0.5, "nu");
        expr_t *x = test_expr_new_named_var_d(2.0, "x");
        expr_t *f = expr_bessel_y(order, x);
        expr_free(order);
        expr_free(x);
        check_roundtrip("to_string: BesselY(nu,x)", f, __LINE__);
    }
    {
        expr_t *x = test_expr_new_named_var_d(3.0, "x");
        expr_t *y = test_expr_new_named_var_d(4.0, "y");
        expr_t *f = expr_hypot(x, y);
        expr_free(x);
        expr_free(y);
        check_roundtrip("to_string: hypot(x,y)", f, __LINE__);
    }
    {
        expr_t *upper[] = {test_expr_new_named_const_d(0.5, "a")};
        expr_t *lower[] = {test_expr_new_named_const_d(1.5, "b")};
        expr_t *x = test_expr_new_named_var_d(0.2, "x");
        expr_t *f = expr_hypergeometric_pFq(1u, (const expr_t *const *)upper, 1u, (const expr_t *const *)lower, x);

        expr_free(x);
        expr_free(lower[0]);
        expr_free(upper[0]);
        check_roundtrip("to_string: general pFq", f, __LINE__);
    }
    {
        expr_t *a = test_expr_new_named_const_d(1.25, "a");
        expr_t *parameters[] = {test_expr_new_named_const_d(0.5, "b1"), test_expr_new_named_const_d(1.5, "b2"),
                                test_expr_new_named_const_d(2.0, "b3")};
        expr_t *c = test_expr_new_named_const_d(2.25, "c");
        expr_t *variables[] = {test_expr_new_named_var_d(0.1, "x"), test_expr_new_named_var_d(0.2, "y"),
                               test_expr_new_named_var_d(0.3, "z")};
        expr_t *f = expr_lauricella_f(a, 3u, (const expr_t *const *)parameters, c, (const expr_t *const *)variables);

        for (size_t i = 0u; i < 3u; ++i) {
            expr_free(variables[i]);
            expr_free(parameters[i]);
        }
        expr_free(c);
        expr_free(a);
        check_roundtrip("to_string: general Lauricella FD", f, __LINE__);
    }
}

static void test_to_string_appell_f1(void)
{
    static const char *const inputs[] = {"{ appell_f1(1, 1, 1, 2, x, y) }", "{ F1(1, 1, 1, 2, x, y) }",
                                         "{ F_1(1, 1, 1, 2, x, y) }", "{ F₁(1, 1, 1, 2, x, y) }", NULL};
    const char *expect_expr = "F₁(1; 1, 1; 2; x, y)";
    const char *expect_func = "appell_f1(1, 1, 1, 2, x, y)";
    const char *expect_TeX = "F_{1}\\left(1; 1, 1; 2; x, y\\right)";

    for (size_t i = 0u; inputs[i]; ++i) {
        expr_t *f = expr_from_string(inputs[i], NULL);
        char *got_expr = f ? expr_to_string(f, style_EXPRESSION) : NULL;
        char *got_func = f ? expr_to_string(f, style_FUNCTION) : NULL;
        char *got_TeX = f ? expr_to_string(f, style_LATEX) : NULL;

        ASSERT_NOT_NULL(f);
        if (got_expr)
            to_string_pass("appell_f1 alias (EXPR)", got_expr, expect_expr);
        else
            to_string_fail(__FILE__, __LINE__, 1, "appell_f1 alias (EXPR)", "(null)", expect_expr);
        if (got_func)
            to_string_pass("appell_f1 alias (FUNCTION)", got_func, expect_func);
        else
            to_string_fail(__FILE__, __LINE__, 1, "appell_f1 alias (FUNCTION)", "(null)", expect_func);
        if (got_TeX)
            to_string_pass("appell_f1 alias (TEX)", got_TeX, expect_TeX);
        else
            to_string_fail(__FILE__, __LINE__, 1, "appell_f1 alias (TEX)", "(null)", expect_TeX);

        free(got_TeX);
        free(got_func);
        free(got_expr);
        expr_free(f);
    }
}

/* ============================================================
 * TEST SUITE RUNNER
 * ============================================================ */

void test_to_string_all(void)
{
    TEST_RUN_SUBTEST(test_to_string_basic_const, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var, NULL);
    TEST_RUN_SUBTEST(test_to_string_basic_var_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_transcendental_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_atan_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_nested_quotient_pow_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_log10_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_exp_unit_fraction_root_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_parsed_exp_unit_fraction_root_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_symbolic_constants_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_symbolic_constant_quotient_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_lambert_w_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_gammainv_TeX, NULL);
    TEST_RUN_SUBTEST(test_to_string_gamma_polygamma_standard_names, NULL);
    TEST_RUN_SUBTEST(test_to_string_non_simple_var_bracketed, NULL);
    TEST_RUN_SUBTEST(test_to_string_addition, NULL);
    TEST_RUN_SUBTEST(test_to_string_wrapped_TeX_aligned_subtraction, NULL);
    TEST_RUN_SUBTEST(test_to_string_wrapped_TeX_distributes_scale, NULL);
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
    TEST_RUN_SUBTEST(test_to_string_appell_f1, NULL);
}

/* ============================================================
 *  make_expr_01        x*x
 * ============================================================ */
static expr_t *make_expr_01(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x); /* x*x */

    expr_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_02        x*x*x
 * ============================================================ */
static expr_t *make_expr_02(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x);  /* x*x      */
    expr_t *t2 = expr_mul(t1, x); /* x*x*x    */

    expr_free(x);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_03        π * x^2
 * ============================================================ */
static expr_t *make_expr_03(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2      */
    expr_t *t2 = expr_mul(pi, t1);   /* π * x^2  */

    expr_free(x);
    expr_free(pi);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_04        x*x + x*x
 * ============================================================ */
static expr_t *make_expr_04(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x);   /* x*x        */
    expr_t *t2 = expr_mul(x, x);   /* x*x        */
    expr_t *t3 = expr_add(t1, t2); /* x*x + x*x  */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_05        x*x + 3*x*x + 7
 * ============================================================ */
static expr_t *make_expr_05(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x);      /* x*x          */
    expr_t *t2 = expr_mul_d(t1, 3.0); /* 3*x*x        */
    expr_t *t3 = expr_add(t1, t2);    /* x*x+3*x*x    */
    expr_t *t4 = expr_add_d(t3, 7.0); /* x*x+3*x*x+7  */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_06        2*x - 5*x
 * ============================================================ */
static expr_t *make_expr_06(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul_d(x, 2.0); /* 2*x   */
    expr_t *t2 = expr_mul_d(x, 5.0); /* 5*x   */
    expr_t *t3 = expr_sub(t1, t2);   /* 2*x-5*x */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_07        x^2 * x^3
 * ============================================================ */
static expr_t *make_expr_07(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2      */
    expr_t *t2 = expr_pow_d(x, 3.0); /* x^3      */
    expr_t *t3 = expr_mul(t1, t2);   /* x^2*x^3  */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_08        x^2 * x * x^4
 * ============================================================ */
static expr_t *make_expr_08(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2        */
    expr_t *t2 = expr_mul(t1, x);    /* x^2*x      */
    expr_t *t3 = expr_pow_d(x, 4.0); /* x^4        */
    expr_t *t4 = expr_mul(t2, t3);   /* x^2*x*x^4  */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_09        x^2 * y^3 * x
 * ============================================================ */
static expr_t *make_expr_09(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2        */
    expr_t *t2 = expr_pow_d(y, 3.0); /* y^3        */
    expr_t *t3 = expr_mul(t1, t2);   /* x^2*y^3    */
    expr_t *t4 = expr_mul(t3, x);    /* x^2*y^3*x  */

    expr_free(x);
    expr_free(y);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_10        3*x^2 * 4*x
 * ============================================================ */
static expr_t *make_expr_10(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_pow_d(x, 2.0);  /* x^2        */
    expr_t *t2 = expr_mul_d(t1, 3.0); /* 3*x^2      */
    expr_t *t3 = expr_mul_d(x, 4.0);  /* 4*x        */
    expr_t *t4 = expr_mul(t2, t3);    /* 3*x^2*4*x  */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_11        3*x * 2*y * x^2   → 6 x^3 y
 * ============================================================ */
static expr_t *make_expr_11(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");

    expr_t *t1 = expr_mul_d(x, 3.0); /* 3*x     */
    expr_t *t2 = expr_mul_d(y, 2.0); /* 2*y     */
    expr_t *t3 = expr_mul(t1, t2);   /* 3*x*2*y */
    expr_t *t4 = expr_pow_d(x, 2.0); /* x^2     */
    expr_t *t5 = expr_mul(t3, t4);   /* 3*x*2*y*x^2 */

    expr_free(x);
    expr_free(y);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_12        x*x*y*x   → x^3 y
 * ============================================================ */
static expr_t *make_expr_12(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");

    expr_t *t1 = expr_mul(x, x);  /* x*x     */
    expr_t *t2 = expr_mul(t1, y); /* x*x*y   */
    expr_t *t3 = expr_mul(t2, x); /* x*x*y*x */

    expr_free(x);
    expr_free(y);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_13        3*x
 * ============================================================ */
static expr_t *make_expr_13(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul_d(x, 3.0); /* 3*x */

    expr_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_14        3*x*x
 * ============================================================ */
static expr_t *make_expr_14(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x);      /* x*x   */
    expr_t *t2 = expr_mul_d(t1, 3.0); /* 3*x*x */

    expr_free(x);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_15        6*x
 * ============================================================ */
static expr_t *make_expr_15(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul_d(x, 6.0); /* 6*x */

    expr_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_16        7*x^2
 * ============================================================ */
static expr_t *make_expr_16(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_pow_d(x, 2.0);  /* x^2     */
    expr_t *t2 = expr_mul_d(t1, 7.0); /* 7*x^2   */

    expr_free(x);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_17        2*x*y
 * ============================================================ */
static expr_t *make_expr_17(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");

    expr_t *t1 = expr_mul(x, y);      /* x*y     */
    expr_t *t2 = expr_mul_d(t1, 2.0); /* 2*x*y   */

    expr_free(x);
    expr_free(y);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_18        sin(x) * cos(x)
 * ============================================================ */
static expr_t *make_expr_18(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);      /* sin(x) */
    expr_t *t2 = expr_cos(x);      /* cos(x) */
    expr_t *t3 = expr_mul(t1, t2); /* sin(x)*cos(x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_19        cos(x) * exp(x)
 * ============================================================ */
static expr_t *make_expr_19(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_cos(x);      /* cos(x) */
    expr_t *t2 = expr_exp(x);      /* exp(x) */
    expr_t *t3 = expr_mul(t1, t2); /* cos(x)*exp(x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_20        exp(x) * x*x   → x^2 * exp(x)
 * ============================================================ */
static expr_t *make_expr_20(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_exp(x);      /* exp(x) */
    expr_t *t2 = expr_mul(x, x);   /* x*x    */
    expr_t *t3 = expr_mul(t2, t1); /* x*x*exp(x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_21        3*exp(x) * x^2
 * ============================================================ */
static expr_t *make_expr_21(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_exp(x);         /* exp(x)     */
    expr_t *t2 = expr_mul_d(t1, 3.0); /* 3*exp(x)   */
    expr_t *t3 = expr_pow_d(x, 2.0);  /* x^2        */
    expr_t *t4 = expr_mul(t2, t3);    /* 3*exp(x)*x^2 */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_22        sin(x) * x^2
 * ============================================================ */
static expr_t *make_expr_22(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);        /* sin(x) */
    expr_t *t2 = expr_pow_d(x, 2.0); /* x^2    */
    expr_t *t3 = expr_mul(t1, t2);   /* sin(x)*x^2 */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_23        x*sin(x)*x   → x^2 * sin(x)
 * ============================================================ */
static expr_t *make_expr_23(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);     /* sin(x) */
    expr_t *t2 = expr_mul(x, t1); /* x*sin(x) */
    expr_t *t3 = expr_mul(t2, x); /* x*sin(x)*x */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_24        exp(sin(x))
 * ============================================================ */
static expr_t *make_expr_24(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);  /* sin(x) */
    expr_t *t2 = expr_exp(t1); /* exp(sin(x)) */

    expr_free(x);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_25        cos(x) * exp(sin(x))
 * ============================================================ */
static expr_t *make_expr_25(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_cos(x);      /* cos(x) */
    expr_t *t2 = expr_sin(x);      /* sin(x) */
    expr_t *t3 = expr_exp(t2);     /* exp(sin(x)) */
    expr_t *t4 = expr_mul(t1, t3); /* cos(x)*exp(sin(x)) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_26        x*x * exp(sin(x))
 * ============================================================ */
static expr_t *make_expr_26(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_mul(x, x);   /* x*x */
    expr_t *t2 = expr_sin(x);      /* sin(x) */
    expr_t *t3 = expr_exp(t2);     /* exp(sin(x)) */
    expr_t *t4 = expr_mul(t1, t3); /* x*x*exp(sin(x)) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_27        exp(sin(x)) * exp(cos(x))
 * ============================================================ */
static expr_t *make_expr_27(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);      /* sin(x) */
    expr_t *t2 = expr_exp(t1);     /* exp(sin(x)) */
    expr_t *t3 = expr_cos(x);      /* cos(x) */
    expr_t *t4 = expr_exp(t3);     /* exp(cos(x)) */
    expr_t *t5 = expr_mul(t2, t4); /* exp(sin(x))*exp(cos(x)) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_28        exp(x^2) * exp(3*x^2)
 * ============================================================ */
static expr_t *make_expr_28(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_pow_d(x, 2.0);  /* x^2       */
    expr_t *t2 = expr_exp(t1);        /* exp(x^2)  */
    expr_t *t3 = expr_mul_d(t1, 3.0); /* 3*x^2     */
    expr_t *t4 = expr_exp(t3);        /* exp(3*x^2) */
    expr_t *t5 = expr_mul(t2, t4);    /* exp(x^2)*exp(3*x^2) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_29        exp(x) * exp(2*x)
 * ============================================================ */
static expr_t *make_expr_29(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_exp(x);        /* exp(x)   */
    expr_t *t2 = expr_mul_d(x, 2.0); /* 2*x      */
    expr_t *t3 = expr_exp(t2);       /* exp(2*x) */
    expr_t *t4 = expr_mul(t1, t3);   /* exp(x)*exp(2*x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_30        exp(sin(x)) * exp(cos(x)) * exp(x)
 * ============================================================ */
static expr_t *make_expr_30(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);      /* sin(x) */
    expr_t *t2 = expr_exp(t1);     /* exp(sin(x)) */
    expr_t *t3 = expr_cos(x);      /* cos(x) */
    expr_t *t4 = expr_exp(t3);     /* exp(cos(x)) */
    expr_t *t5 = expr_exp(x);      /* exp(x) */
    expr_t *t6 = expr_mul(t2, t4); /* exp(sin(x))*exp(cos(x)) */
    expr_t *t7 = expr_mul(t6, t5); /* exp(sin(x))*exp(cos(x))*exp(x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    expr_free(t5);
    expr_free(t6);
    return t7;
}

/* ============================================================
 *  make_expr_31        π * sin(x)
 * ============================================================ */
static expr_t *make_expr_31(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");

    expr_t *t1 = expr_sin(x);      /* sin(x)   */
    expr_t *t2 = expr_mul(pi, t1); /* π*sin(x) */

    expr_free(x);
    expr_free(pi);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_32        τ * cos(x)
 * ============================================================ */
static expr_t *make_expr_32(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_cos(x);       /* cos(x)   */
    expr_t *t2 = expr_mul(tau, t1); /* τ*cos(x) */

    expr_free(x);
    expr_free(tau);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_33        e * x^2
 * ============================================================ */
static expr_t *make_expr_33(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *e = test_expr_new_named_const_qf(QF_E, "e");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2    */
    expr_t *t2 = expr_mul(e, t1);    /* e*x^2  */

    expr_free(x);
    expr_free(e);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_34        π * τ * e
 * ============================================================ */
static expr_t *make_expr_34(void)
{
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");
    expr_t *e = test_expr_new_named_const_qf(QF_E, "e");

    expr_t *t1 = expr_mul(pi, tau); /* π*τ   */
    expr_t *t2 = expr_mul(t1, e);   /* π*τ*e */

    expr_free(pi);
    expr_free(tau);
    expr_free(e);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_35        π * x * τ * y
 * ============================================================ */
static expr_t *make_expr_35(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_mul(pi, x);   /* π*x     */
    expr_t *t2 = expr_mul(t1, tau); /* π*x*τ   */
    expr_t *t3 = expr_mul(t2, y);   /* π*x*τ*y */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_36        e^(x) * π
 * ============================================================ */
static expr_t *make_expr_36(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");

    expr_t *t1 = expr_exp(x);      /* exp(x) */
    expr_t *t2 = expr_mul(t1, pi); /* exp(x)*π */

    expr_free(x);
    expr_free(pi);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_37        τ * exp(x^2)
 * ============================================================ */
static expr_t *make_expr_37(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_pow_d(x, 2.0); /* x^2        */
    expr_t *t2 = expr_exp(t1);       /* exp(x^2)   */
    expr_t *t3 = expr_mul(tau, t2);  /* τ*exp(x^2) */

    expr_free(x);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_38        e * sin(x) * cos(y)
 * ============================================================ */
static expr_t *make_expr_38(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *e = test_expr_new_named_const_qf(QF_E, "e");

    expr_t *t1 = expr_sin(x);      /* sin(x) */
    expr_t *t2 = expr_cos(y);      /* cos(y) */
    expr_t *t3 = expr_mul(t1, t2); /* sin(x)*cos(y) */
    expr_t *t4 = expr_mul(e, t3);  /* e*sin(x)*cos(y) */

    expr_free(x);
    expr_free(y);
    expr_free(e);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_39        π * exp(τ * x)
 * ============================================================ */
static expr_t *make_expr_39(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_mul(tau, x); /* τ*x        */
    expr_t *t2 = expr_exp(t1);     /* exp(τ*x)   */
    expr_t *t3 = expr_mul(pi, t2); /* π*exp(τ*x) */

    expr_free(x);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_40        e^(π*x) * τ
 * ============================================================ */
static expr_t *make_expr_40(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_mul(pi, x);   /* π*x      */
    expr_t *t2 = expr_exp(t1);      /* exp(π*x) */
    expr_t *t3 = expr_mul(t2, tau); /* exp(π*x)*τ */

    expr_free(x);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_41        sin(π * x)
 * ============================================================ */
static expr_t *make_expr_41(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");

    expr_t *t1 = expr_mul(pi, x); /* π*x       */
    expr_t *t2 = expr_sin(t1);    /* sin(π*x)  */

    expr_free(x);
    expr_free(pi);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_42        cos(τ * x)
 * ============================================================ */
static expr_t *make_expr_42(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_mul(tau, x); /* τ*x       */
    expr_t *t2 = expr_cos(t1);     /* cos(τ*x)  */

    expr_free(x);
    expr_free(tau);
    expr_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_43        exp(π * τ * x)
 * ============================================================ */
static expr_t *make_expr_43(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_mul(pi, tau); /* π*τ      */
    expr_t *t2 = expr_mul(t1, x);   /* π*τ*x    */
    expr_t *t3 = expr_exp(t2);      /* exp(π*τ*x) */

    expr_free(x);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_44        sin(x) + cos(x) + exp(x)
 * ============================================================ */
static expr_t *make_expr_44(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");

    expr_t *t1 = expr_sin(x);      /* sin(x) */
    expr_t *t2 = expr_cos(x);      /* cos(x) */
    expr_t *t3 = expr_add(t1, t2); /* sin(x)+cos(x) */
    expr_t *t4 = expr_exp(x);      /* exp(x) */
    expr_t *t5 = expr_add(t3, t4); /* sin(x)+cos(x)+exp(x) */

    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_45        x + y + π + τ + e
 * ============================================================ */
static expr_t *make_expr_45(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");
    expr_t *e = test_expr_new_named_const_qf(QF_E, "e");

    expr_t *t1 = expr_add(x, y);    /* x+y     */
    expr_t *t2 = expr_add(t1, pi);  /* x+y+π   */
    expr_t *t3 = expr_add(t2, tau); /* x+y+π+τ */
    expr_t *t4 = expr_add(t3, e);   /* x+y+π+τ+e */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(e);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_46        x*y + π*x + τ*y + e
 * ============================================================ */
static expr_t *make_expr_46(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");
    expr_t *e = test_expr_new_named_const_qf(QF_E, "e");

    expr_t *t1 = expr_mul(x, y);   /* x*y     */
    expr_t *t2 = expr_mul(pi, x);  /* π*x     */
    expr_t *t3 = expr_mul(tau, y); /* τ*y     */
    expr_t *t4 = expr_add(t1, t2); /* x*y + π*x */
    expr_t *t5 = expr_add(t4, t3); /* x*y + π*x + τ*y */
    expr_t *t6 = expr_add(t5, e);  /* x*y + π*x + τ*y + e */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(e);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    expr_free(t5);
    return t6;
}

/* ============================================================
 *  make_expr_47        (x + π) * (y + τ)
 * ============================================================ */
static expr_t *make_expr_47(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_add(x, pi);  /* x+π */
    expr_t *t2 = expr_add(y, tau); /* y+τ */
    expr_t *t3 = expr_mul(t1, t2); /* (x+π)*(y+τ) */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_48        exp(x + π) * exp(y + τ)
 * ============================================================ */
static expr_t *make_expr_48(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_add(x, pi);  /* x+π */
    expr_t *t2 = expr_exp(t1);     /* exp(x+π) */
    expr_t *t3 = expr_add(y, tau); /* y+τ */
    expr_t *t4 = expr_exp(t3);     /* exp(y+τ) */
    expr_t *t5 = expr_mul(t2, t4); /* exp(x+π)*exp(y+τ) */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_49        sin(x + π) * cos(y + τ)
 * ============================================================ */
static expr_t *make_expr_49(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_add(x, pi);  /* x+π */
    expr_t *t2 = expr_sin(t1);     /* sin(x+π) */
    expr_t *t3 = expr_add(y, tau); /* y+τ */
    expr_t *t4 = expr_cos(t3);     /* cos(y+τ) */
    expr_t *t5 = expr_mul(t2, t4); /* sin(x+π)*cos(y+τ) */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_50        exp(sin(x + π) + cos(y + τ))
 * ============================================================ */
static expr_t *make_expr_50(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *y = test_expr_new_named_var_d(1.25, "y");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "@tau");

    expr_t *t1 = expr_add(x, pi);  /* x+π */
    expr_t *t2 = expr_sin(t1);     /* sin(x+π) */
    expr_t *t3 = expr_add(y, tau); /* y+τ */
    expr_t *t4 = expr_cos(t3);     /* cos(y+τ) */
    expr_t *t5 = expr_add(t2, t4); /* sin(x+π)+cos(y+τ) */
    expr_t *t6 = expr_exp(t5);     /* exp(sin(x+π)+cos(y+τ)) */

    expr_free(x);
    expr_free(y);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    expr_free(t4);
    expr_free(t5);
    return t6;
}

void test_expressions(void)
{
    /* ============================================================
     *  Test table (all 50 entries)
     * ============================================================ */
    struct {
        const char *src;
        expr_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line; /* NEW: source line of this test entry */
    } tests[] = {
        /* 01 */
        {"x*x", make_expr_01, "{ x² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^2\n"
         "return expr(x)",
         __LINE__},

        /* 02 */
        {"x*x*x", make_expr_02, "{ x³ | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^3\n"
         "return expr(x)",
         __LINE__},

        /* 03 */
        {"π * x^2", make_expr_03, "{ πx² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = π*x^2\n"
         "return expr(x)",
         __LINE__},

        /* 04 */
        {"x*x + x*x", make_expr_04, "{ 2x² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 2*x^2\n"
         "return expr(x)",
         __LINE__},

        /* 05 */
        {"x*x + 3*x*x + 7", make_expr_05, "{ 4x² + 7 | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 4*x^2 + 7\n"
         "return expr(x)",
         __LINE__},

        /* 06 */
        {"2*x - 5*x", make_expr_06, "{ -3x | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = -3*x\n"
         "return expr(x)",
         __LINE__},

        /* 07 */
        {"x^2 * x^3", make_expr_07, "{ x⁵ | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^5\n"
         "return expr(x)",
         __LINE__},

        /* 08 */
        {"x^2 * x * x^4", make_expr_08, "{ x⁷ | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^7\n"
         "return expr(x)",
         __LINE__},

        /* 09 */
        {"x^2 * y^3 * x", make_expr_09, "{ x³y³ | x = 1.25, y = 1.25 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "expr(x,y) = x^3*y^3\n"
         "return expr(x,y)",
         __LINE__},

        /* 10 */
        {"3*x^2 * 4*x", make_expr_10, "{ 12x³ | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 12*x^3\n"
         "return expr(x)",
         __LINE__},

        /* 11 */
        {"3*x * 2*y * x^2", make_expr_11, "{ 6x³y | x = 1.25, y = 1.25 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "expr(x,y) = 6*x^3*y\n"
         "return expr(x,y)",
         __LINE__},

        /* 12 */
        {"x*x*y*x", make_expr_12, "{ x³y | x = 1.25, y = 1.25 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "expr(x,y) = x^3*y\n"
         "return expr(x,y)",
         __LINE__},

        /* 13 */
        {"3*x", make_expr_13, "{ 3x | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 3*x\n"
         "return expr(x)",
         __LINE__},

        /* 14 */
        {"3*x*x", make_expr_14, "{ 3x² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 3*x^2\n"
         "return expr(x)",
         __LINE__},

        /* 15 */
        {"6*x", make_expr_15, "{ 6x | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 6*x\n"
         "return expr(x)",
         __LINE__},

        /* 16 */
        {"7*x^2", make_expr_16, "{ 7x² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 7*x^2\n"
         "return expr(x)",
         __LINE__},

        /* 17 */
        {"2*x*y", make_expr_17, "{ 2xy | x = 1.25, y = 1.25 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "expr(x,y) = 2*x*y\n"
         "return expr(x,y)",
         __LINE__},

        /* 18 */
        {"sin(x)*cos(x)", make_expr_18, "{ ½·sin(2x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = sin(2*x)/2\n"
         "return expr(x)",
         __LINE__},

        /* 19 */
        {"cos(x)*exp(x)", make_expr_19, "{ cos(x)·exp(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = cos(x)*exp(x)\n"
         "return expr(x)",
         __LINE__},

        /* 20 */
        {"exp(x)*x*x", make_expr_20, "{ x²·exp(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^2*exp(x)\n"
         "return expr(x)",
         __LINE__},

        /* 21 */
        {"3*exp(x)*x^2", make_expr_21, "{ 3x²·exp(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = 3*x^2*exp(x)\n"
         "return expr(x)",
         __LINE__},

        /* 22 */
        {"sin(x)*x^2", make_expr_22, "{ x²·sin(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^2*sin(x)\n"
         "return expr(x)",
         __LINE__},

        /* 23 */
        {"x*sin(x)*x", make_expr_23, "{ x²·sin(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^2*sin(x)\n"
         "return expr(x)",
         __LINE__},

        /* 24 */
        {"exp(sin(x))", make_expr_24, "{ exp(sin(x)) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = exp(sin(x))\n"
         "return expr(x)",
         __LINE__},

        /* 25 */
        {"cos(x)*exp(sin(x))", make_expr_25, "{ cos(x)·exp(sin(x)) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = cos(x)*exp(sin(x))\n"
         "return expr(x)",
         __LINE__},

        /* 26 */
        {"x*x*exp(sin(x))", make_expr_26, "{ x²·exp(sin(x)) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = x^2*exp(sin(x))\n"
         "return expr(x)",
         __LINE__},

        /* 27 */
        {"exp(sin(x))*exp(cos(x))", make_expr_27, "{ exp(sin(x) + cos(x)) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = exp(sin(x) + cos(x))\n"
         "return expr(x)",
         __LINE__},

        /* 28 */
        {"exp(x^2)*exp(3*x^2)", make_expr_28, "{ exp(4x²) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = exp(4*x^2)\n"
         "return expr(x)",
         __LINE__},

        /* 29 */
        {"exp(x)*exp(2*x)", make_expr_29, "{ exp(3x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = exp(3*x)\n"
         "return expr(x)",
         __LINE__},

        /* 30 */
        {"exp(sin(x))*exp(cos(x))*exp(x)", make_expr_30, "{ exp(sin(x) + cos(x) + x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = exp(sin(x) + cos(x) + x)\n"
         "return expr(x)",
         __LINE__},

        /* 31 */
        {"π*sin(x)", make_expr_31, "{ π·sin(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = π*sin(x)\n"
         "return expr(x)",
         __LINE__},

        /* 32 */
        {"τ*cos(x)", make_expr_32, "{ τ·cos(x) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = τ*cos(x)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 33 */
        {"e*x^2", make_expr_33, "{ ex² | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = e*x^2\n"
         "return expr(x)",
         __LINE__},

        /* 34 */
        {"π*τ*e", make_expr_34, "{ πτe | ; τ = 6.283185307179586476925286766559011 }",
         "τ = 6.283185307179586476925286766559011\n"
         "expr(τ) = π*τ*e\n"
         "return expr(τ)",
         __LINE__},

        /* 35 */
        {"π*x*τ*y", make_expr_35, "{ πτxy | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = π*τ*x*y\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 36 */
        {"exp(x)*π", make_expr_36, "{ π·exp(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = π*exp(x)\n"
         "return expr(x)",
         __LINE__},

        /* 37 */
        {"τ*exp(x^2)", make_expr_37, "{ τ·exp(x²) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = τ*exp(x^2)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 38 */
        {"e*sin(x)*cos(y)", make_expr_38, "{ e·sin(x)·cos(y) | x = 1.25, y = 1.25 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "expr(x,y) = e*sin(x)*cos(y)\n"
         "return expr(x,y)",
         __LINE__},

        /* 39 */
        {"π*exp(τ*x)", make_expr_39, "{ π·exp(τx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = π*exp(τ*x)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 40 */
        {"exp(π*x)*τ", make_expr_40, "{ τ·exp(πx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = τ*exp(π*x)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 41 */
        {"sin(π*x)", make_expr_41, "{ sin(πx) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = sin(π*x)\n"
         "return expr(x)",
         __LINE__},

        /* 42 */
        {"cos(τ*x)", make_expr_42, "{ cos(τx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = cos(τ*x)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 43 */
        {"exp(π*τ*x)", make_expr_43, "{ exp(πτx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,τ) = exp(π*τ*x)\n"
         "return expr(x,τ)",
         __LINE__},

        /* 44 */
        {"sin(x)+cos(x)+exp(x)", make_expr_44, "{ sin(x) + cos(x) + exp(x) | x = 1.25 }",
         "x = 1.25\n"
         "expr(x) = sin(x) + cos(x) + exp(x)\n"
         "return expr(x)",
         __LINE__},

        /* 45 */
        {"x + y + π + τ + e", make_expr_45,
         "{ x + y + e + π + τ | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = x + y + e + π + τ\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 46 */
        {"x*y + π*x + τ*y + e", make_expr_46,
         "{ xy + πx + τy + e | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = e + π*x + τ*y + x*y\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 47 */
        {"(x+π)*(y+τ)", make_expr_47,
         "{ (x + π)·(y + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = (x + π)*(y + τ)\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 48 */
        {"exp(x+π)*exp(y+τ)", make_expr_48,
         "{ exp(x + y + π + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = exp(x + y + π + τ)\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 49 */
        {"sin(x+π)*cos(y+τ)", make_expr_49,
         "{ sin(x + π)·cos(y + τ) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = sin(x + π)*cos(y + τ)\n"
         "return expr(x,y,τ)",
         __LINE__},

        /* 50 */
        {"exp(sin(x+π) + cos(y+τ))", make_expr_50,
         "{ exp(sin(x + π) + cos(y + τ)) | x = 1.25, y = 1.25; τ = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "y = 1.25\n"
         "τ = 6.283185307179586476925286766559011\n"
         "expr(x,y,τ) = exp(sin(x + π) + cos(y + τ))\n"
         "return expr(x,y,τ)",
         __LINE__},
    };

    /* ============================================================
     *  Test loop — formatted with bold PASS/FAIL and file:line
     * ============================================================ */
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        expr_t *f = tests[i].make();
        expr_t *simp = expr_simplify(f);

        char *got_expr = expr_to_string(simp, style_EXPRESSION);
        char *got_func = expr_to_string(simp, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        /* ---------------- EXPR block ---------------- */
        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        /* ---------------- FUNC block ---------------- */
        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
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
        expr_free(simp);
        expr_free(f);
    }
}

/* ============================================================
 *  Builders for unnamed-variable tests (U01–U06)
 * ============================================================ */

/* U01: x₀² */
expr_t *make_expr_u01(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *f = expr_mul(x, x);
    expr_free(x);
    return f;
}

/* U02: x₀³ */
expr_t *make_expr_u02(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *t1 = expr_mul(x, x);
    expr_t *f = expr_mul(t1, x);
    expr_free(x);
    expr_free(t1);
    return f;
}

/* U03: x₀³x₁³  (mirrors test 09, but with unnamed vars) */
expr_t *make_expr_u03(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *y = test_expr_new_var_d(1.25);
    expr_t *t1 = expr_pow_d(x, 2.0);
    expr_t *t2 = expr_pow_d(y, 3.0);
    expr_t *t3 = expr_mul(t1, t2);
    expr_t *f = expr_mul(t3, x);
    expr_free(x);
    expr_free(y);
    expr_free(t1);
    expr_free(t2);
    expr_free(t3);
    return f;
}

/* U04: 2x₀²  (coefficient stays numeric after simplification) */
expr_t *make_expr_u04(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *t1 = expr_mul(x, x);
    expr_t *t2 = expr_mul(x, x);
    expr_t *f = expr_add(t1, t2);
    expr_free(x);
    expr_free(t1);
    expr_free(t2);
    return f;
}

/* U05: sin(x₀)·cos(x₀) */
expr_t *make_expr_u05(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *sx = expr_sin(x);
    expr_t *cx = expr_cos(x);
    expr_t *f = expr_mul(sx, cx);
    expr_free(x);
    expr_free(sx);
    expr_free(cx);
    return f;
}

/* U06: exp(sin(x₀) + cos(x₀))  (exp merge) */
expr_t *make_expr_u06(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *sx = expr_sin(x);
    expr_t *cx = expr_cos(x);
    expr_t *t1 = expr_exp(sx);
    expr_t *t2 = expr_exp(cx);
    expr_t *f = expr_mul(t1, t2);
    expr_free(x);
    expr_free(sx);
    expr_free(cx);
    expr_free(t1);
    expr_free(t2);
    return f;
}

/* ============================================================
 *  Builders for manually-subscripted constant tests (C01–C04)
 *
 *  Callers pass "c\xE2\x82\x80" (c₀) and "c\xE2\x82\x81" (c₁)
 *  as the name argument to expr_new_named_const so the names are
 *  simple (letter + subscript digit) — they won't be bracketed.
 * ============================================================ */

/* C01: c₀x₀²  (named const × unnamed var²) */
expr_t *make_expr_c01(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *c = test_expr_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *f = expr_mul(c, x2);
    expr_free(x);
    expr_free(c);
    expr_free(x2);
    return f;
}

/* C02: c₀·sin(x₀)  (named const × function — needs separator) */
expr_t *make_expr_c02(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *c = test_expr_new_named_const_qf(QF_E, "c\xE2\x82\x80");
    expr_t *sx = expr_sin(x);
    expr_t *f = expr_mul(c, sx);
    expr_free(x);
    expr_free(c);
    expr_free(sx);
    return f;
}

/* C03: x₀ + x₁ + c₀ */
expr_t *make_expr_c03(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *y = test_expr_new_var_d(1.25);
    expr_t *c = test_expr_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    expr_t *t1 = expr_add(x, y);
    expr_t *f = expr_add(t1, c);
    expr_free(x);
    expr_free(y);
    expr_free(c);
    expr_free(t1);
    return f;
}

/* C04: c₀x₀ + c₁  (two named consts with unnamed var; tests multi-const bindings) */
expr_t *make_expr_c04(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *c0 = test_expr_new_named_const_qf(QF_PI, "c\xE2\x82\x80");
    expr_t *c1 = test_expr_new_named_const_qf(QF_E, "c\xE2\x82\x81");
    expr_t *t1 = expr_mul(c0, x);
    expr_t *f = expr_add(t1, c1);
    expr_free(x);
    expr_free(c0);
    expr_free(c1);
    expr_free(t1);
    return f;
}

/* ============================================================
 *  Builders for multi-character name tests (L01–L09)
 * ============================================================ */

/* L01: [radius]² */
expr_t *make_expr_l01(void)
{
    expr_t *r = test_expr_new_named_var_d(1.25, "radius");
    expr_t *f = expr_pow_d(r, 2.0);
    expr_free(r);
    return f;
}

/* L02: [base]·[height]  (two multi-char vars — separator needed) */
expr_t *make_expr_l02(void)
{
    expr_t *base = test_expr_new_named_var_d(1.25, "base");
    expr_t *height = test_expr_new_named_var_d(1.25, "height");
    expr_t *f = expr_mul(base, height);
    expr_free(base);
    expr_free(height);
    return f;
}

/* L03: [pi]·[radius]²  (multi-char named const × multi-char named var²) */
expr_t *make_expr_l03(void)
{
    expr_t *r = test_expr_new_named_var_d(1.25, "radius");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "pi");
    expr_t *r2 = expr_pow_d(r, 2.0);
    expr_t *f = expr_mul(pi, r2);
    expr_free(r);
    expr_free(pi);
    expr_free(r2);
    return f;
}

/* L04: π·[radius]²  (@pi → π is simple; radius is not — separator needed) */
expr_t *make_expr_l04(void)
{
    expr_t *r = test_expr_new_named_var_d(1.25, "radius");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *r2 = expr_pow_d(r, 2.0);
    expr_t *f = expr_mul(pi, r2);
    expr_free(r);
    expr_free(pi);
    expr_free(r2);
    return f;
}

/* L05: sin([theta])·cos([theta]) */
expr_t *make_expr_l05(void)
{
    expr_t *t = test_expr_new_named_var_d(1.25, "theta");
    expr_t *st = expr_sin(t);
    expr_t *ct = expr_cos(t);
    expr_t *f = expr_mul(st, ct);
    expr_free(t);
    expr_free(st);
    expr_free(ct);
    return f;
}

/* L06: [pi]·[tau]·x  (two multi-char consts + one single-char var) */
expr_t *make_expr_l06(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "pi");
    expr_t *tau = test_expr_new_named_const_qf(QF_2PI, "tau");
    expr_t *t1 = expr_mul(pi, tau);
    expr_t *f = expr_mul(t1, x);
    expr_free(x);
    expr_free(pi);
    expr_free(tau);
    expr_free(t1);
    return f;
}

/* L07: [my var]²  (space in name) */
expr_t *make_expr_l07(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "my var");
    expr_t *f = expr_pow_d(x, 2.0);
    expr_free(x);
    return f;
}

/* L08: [2pi]·x  (name starting with a digit) */
expr_t *make_expr_l08(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x");
    expr_t *c = test_expr_new_named_const_qf(QF_PI, "2pi");
    expr_t *f = expr_mul(c, x);
    expr_free(x);
    expr_free(c);
    return f;
}

/* L09: [x']²  (non-alphanumeric character — apostrophe/prime) */
expr_t *make_expr_l09(void)
{
    expr_t *x = test_expr_new_named_var_d(1.25, "x'");
    expr_t *f = expr_pow_d(x, 2.0);
    expr_free(x);
    return f;
}

/* ============================================================
 *  test_expressions_unnamed
 * ============================================================ */
void test_expressions_unnamed(void)
{
    struct {
        const char *src;
        expr_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* U01 */
        {"x*x (unnamed)", make_expr_u01, "{ x₀² | x₀ = 1.25 }",
         "x₀ = 1.25\n"
         "expr(x₀) = x₀^2\n"
         "return expr(x₀)",
         __LINE__},

        /* U02 */
        {"x*x*x (unnamed)", make_expr_u02, "{ x₀³ | x₀ = 1.25 }",
         "x₀ = 1.25\n"
         "expr(x₀) = x₀^3\n"
         "return expr(x₀)",
         __LINE__},

        /* U03 */
        {"x^2*y^3*x (unnamed)", make_expr_u03, "{ x₀³x₁³ | x₀ = 1.25, x₁ = 1.25 }",
         "x₀ = 1.25\n"
         "x₁ = 1.25\n"
         "expr(x₀,x₁) = x₀^3*x₁^3\n"
         "return expr(x₀,x₁)",
         __LINE__},

        /* U04 */
        {"x*x + x*x (unnamed)", make_expr_u04, "{ 2x₀² | x₀ = 1.25 }",
         "x₀ = 1.25\n"
         "expr(x₀) = 2*x₀^2\n"
         "return expr(x₀)",
         __LINE__},

        /* U05 */
        {"sin(x)*cos(x) (unnamed)", make_expr_u05, "{ ½·sin(2x₀) | x₀ = 1.25 }",
         "x₀ = 1.25\n"
         "expr(x₀) = sin(2*x₀)/2\n"
         "return expr(x₀)",
         __LINE__},

        /* U06 */
        {"exp(sin(x))*exp(cos(x)) (unnamed)", make_expr_u06, "{ exp(sin(x₀) + cos(x₀)) | x₀ = 1.25 }",
         "x₀ = 1.25\n"
         "expr(x₀) = exp(sin(x₀) + cos(x₀))\n"
         "return expr(x₀)",
         __LINE__},

        /* C01 */
        {"c₀*x₀^2 (named const, unnamed var)", make_expr_c01,
         "{ c₀x₀² | x₀ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
         "x₀ = 1.25\n"
         "c₀ = 3.141592653589793238462643383279505\n"
         "expr(x₀,c₀) = c₀*x₀^2\n"
         "return expr(x₀,c₀)",
         __LINE__},

        /* C02 */
        {"c₀*sin(x₀) (named const, unnamed var)", make_expr_c02,
         "{ c₀·sin(x₀) | x₀ = 1.25; c₀ = 2.718281828459045235360287471352664 }",
         "x₀ = 1.25\n"
         "c₀ = 2.718281828459045235360287471352664\n"
         "expr(x₀,c₀) = c₀*sin(x₀)\n"
         "return expr(x₀,c₀)",
         __LINE__},

        /* C03 */
        {"x₀ + x₁ + c₀", make_expr_c03,
         "{ x₀ + x₁ + c₀ | x₀ = 1.25, x₁ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
         "x₀ = 1.25\n"
         "x₁ = 1.25\n"
         "c₀ = 3.141592653589793238462643383279505\n"
         "expr(x₀,x₁,c₀) = x₀ + x₁ + c₀\n"
         "return expr(x₀,x₁,c₀)",
         __LINE__},

        /* C04 */
        {"c₀*x₀ + c₁ (two named consts, unnamed var)", make_expr_c04,
         "{ c₀x₀ + c₁ | x₀ = 1.25; c₁ = 2.718281828459045235360287471352664, c₀ = 3.141592653589793238462643383279505 "
         "}",
         "x₀ = 1.25\n"
         "c₁ = 2.718281828459045235360287471352664\n"
         "c₀ = 3.141592653589793238462643383279505\n"
         "expr(x₀,c₁,c₀) = c₁ + c₀*x₀\n"
         "return expr(x₀,c₁,c₀)",
         __LINE__},
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        expr_t *f = tests[i].make();
        expr_t *simp = expr_simplify(f);

        char *got_expr = expr_to_string(simp, style_EXPRESSION);
        char *got_func = expr_to_string(simp, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
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
        expr_free(simp);
        expr_free(f);
    }
}

/* ============================================================
 *  test_expressions_longname
 * ============================================================ */
void test_expressions_longname(void)
{
    struct {
        const char *src;
        expr_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* L01 */
        {"radius^2", make_expr_l01, "{ [radius]² | [radius] = 1.25 }",
         "[radius] = 1.25\n"
         "expr([radius]) = [radius]^2\n"
         "return expr([radius])",
         __LINE__},

        /* L02 */
        {"base * height", make_expr_l02, "{ [base]·[height] | [base] = 1.25, [height] = 1.25 }",
         "[base] = 1.25\n"
         "[height] = 1.25\n"
         "expr([base],[height]) = [base]*[height]\n"
         "return expr([base],[height])",
         __LINE__},

        /* L03 */
        {"pi * radius^2", make_expr_l03, "{ [pi]·[radius]² | [radius] = 1.25 }",
         "[radius] = 1.25\n"
         "expr([radius]) = [pi]*[radius]^2\n"
         "return expr([radius])",
         __LINE__},

        /* L04 */
        {"@pi * radius^2", make_expr_l04, "{ π·[radius]² | [radius] = 1.25 }",
         "[radius] = 1.25\n"
         "expr([radius]) = π*[radius]^2\n"
         "return expr([radius])",
         __LINE__},

        /* L05 */
        {"sin(theta)*cos(theta)", make_expr_l05, "{ sin([theta])·cos([theta]) | [theta] = 1.25 }",
         "[theta] = 1.25\n"
         "expr([theta]) = sin([theta])*cos([theta])\n"
         "return expr([theta])",
         __LINE__},

        /* L06 */
        {"pi * tau * x", make_expr_l06, "{ [pi]·[tau]·x | x = 1.25; [tau] = 6.283185307179586476925286766559011 }",
         "x = 1.25\n"
         "[tau] = 6.283185307179586476925286766559011\n"
         "expr(x,[tau]) = [pi]*[tau]*x\n"
         "return expr(x,[tau])",
         __LINE__},

        /* L07: space in name */
        {"\"my var\"^2", make_expr_l07, "{ [my var]² | [my var] = 1.25 }",
         "[my var] = 1.25\n"
         "expr([my var]) = [my var]^2\n"
         "return expr([my var])",
         __LINE__},

        /* L08: name starting with a digit */
        {"\"2pi\" * x", make_expr_l08, "{ [2pi]·x | x = 1.25; [2pi] = 3.141592653589793238462643383279505 }",
         "x = 1.25\n"
         "[2pi] = 3.141592653589793238462643383279505\n"
         "expr(x,[2pi]) = [2pi]*x\n"
         "return expr(x,[2pi])",
         __LINE__},

        /* L09: non-alphanumeric character (apostrophe/prime) */
        {"\"x'\"^2", make_expr_l09, "{ [x']² | [x'] = 1.25 }",
         "[x'] = 1.25\n"
         "expr([x']) = [x']^2\n"
         "return expr([x'])",
         __LINE__},
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        expr_t *f = tests[i].make();

        char *got_expr = expr_to_string(f, style_EXPRESSION);
        char *got_func = expr_to_string(f, style_FUNCTION);
        char *expected_func_c = test_legacy_function_expect_to_c(tests[i].expected_func);
        const char *expected_func = expected_func_c ? expected_func_c : tests[i].expected_func;

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET, tests[i].src, __FILE__,
                   tests[i].line);
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
        expr_free(f);
    }
}

/* ============================================================
 *  test_expr_t_from_string — dedicated from_string test group
 * ============================================================ */

/* Round-trip helper: build a expr_t, convert to expr string, parse it back,
 * and verify the evaluated value matches the original. */
void check_roundtrip(const char *label, expr_t *f, int line)
{
    char *s = expr_to_string(f, style_EXPRESSION);
    expr_t *g = expr_from_string(s, NULL);

    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (from_string returned NULL) %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  string " C_RESET "%s\n\n", s);
        TEST_FAIL();
        free(s);
        expr_free(f);
        return;
    }

    qfloat_t expect = expr_eval_qf(f);
    qfloat_t got = expr_eval_qf(g);
    qfloat_t diff = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d = fabs(qf_to_double(expect));
    double rel_err = (exp_d > 0) ? abs_err / exp_d : abs_err;

    const double TOL = 2e-14; /* round-trip through double in binding values */
    if (abs_err < TOL || rel_err < TOL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  string  " C_RESET "%s\n\n", s);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s value mismatch %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        qf_printf(C_BOLD "  got     " C_RESET "%.34q\n", got);
        qf_printf(C_BOLD "  expect  " C_RESET "%.34q\n", expect);
        printf("\n");
        TEST_FAIL();
    }

    free(s);
    expr_free(g);
    expr_free(f);
}

/* Check that parsing an explicit string gives a specific evaluated value. */
void check_parse_val(const char *label, const char *s, double expect_d, int line)
{
    expr_t *g = expr_from_string(s, NULL);
    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (NULL) %s:%d:1\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }
    double got = expr_eval_d(g);
    double err = fabs(got - expect_d);
    double rel = (fabs(expect_d) > 0) ? err / fabs(expect_d) : err;
    const double TOL = 2e-14;
    if (err < TOL || rel < TOL) {
        char *parsed = expr_to_string(g, style_EXPRESSION);
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input   " C_RESET "%s\n", s);
        printf(C_BOLD "  parsed  " C_RESET "%s\n\n", parsed ? parsed : "(null)");
        free(parsed);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        printf(C_BOLD "  got     " C_RESET "%.17g\n", got);
        printf(C_BOLD "  expect  " C_RESET "%.17g\n", expect_d);
        printf("\n");
        TEST_FAIL();
    }
    expr_free(g);
}

/* Check that parsing a string returns NULL (expected error path).
 * Note: expr_from_string prints diagnostics to stderr for error cases. */
void check_parse_null(const char *label, const char *s, int line)
{
    expr_t *g = expr_from_string(s, NULL);
    if (!g) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n\n", label);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (expected NULL) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        expr_free(g);
    }
}

void check_parse_null_stderr_contains(const char *label, const char *s, const char *expected_substring, int line)
{
    const char *capture_path = NULL;
    int saved_stderr;
    expr_t *g;
    FILE *f;
    long size;
    char *buf;
    size_t nread;

    saved_stderr = test_case_begin_stderr_capture("expr-from-string-stderr.txt", &capture_path);
    if (saved_stderr < 0 || !capture_path) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture unavailable) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    g = expr_from_string(s, NULL);

    if (!test_case_end_stderr_capture(saved_stderr)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr restore failed) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        if (g)
            expr_free(g);
        return;
    }

    if (g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (expected NULL) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        expr_free(g);
        return;
    }

    f = fopen(capture_path, "rb");
    if (!f) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture unreadable) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture seek failed) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture size failed) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    buf = malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (stderr capture alloc failed) %s:%d:1\n\n", label, __FILE__, line);
        TEST_FAIL();
        return;
    }

    nread = fread(buf, 1, (size_t)size, f);
    buf[nread] = '\0';
    fclose(f);

    if (expected_substring && *expected_substring && !strstr(buf, expected_substring)) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (missing stderr substring) %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  expected stderr to contain " C_RESET "%s\n", expected_substring);
        printf(C_BOLD "  got stderr             " C_RESET "%s\n\n", buf);
        TEST_FAIL();
        free(buf);
        return;
    }

    printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
    if (expected_substring && *expected_substring)
        printf(C_BOLD "  stderr  " C_RESET "contains \"%s\"\n\n", expected_substring);
    else
        printf("\n");
    free(buf);
}

/* ---- Legacy pure-constant parse format: { name = val } ---- */
