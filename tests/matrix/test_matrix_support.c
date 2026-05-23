#include "test_matrix.h"
#include "internal/dval_internal.h"

char current_matrix_input_label[128];
static matrix_t *current_matrix_input = NULL;

static matrix_t *clone_matrix_snapshot(const matrix_t *A)
{
    if (!A)
        return NULL;

    switch (mat_typeof(A))
    {
    case MAT_TYPE_NUMBER:
        /* Numeric tests already print concrete matrices inline; avoid
         * retaining extra numeric snapshots in the harness. */
        return NULL;
    case MAT_TYPE_DVAL:
        return mat_to_dense(A);
    }

    return NULL;
}

matrix_t *test_mat_create_d(size_t rows, size_t cols, const double *values)
{
    size_t count, i;
    number_t *tmp;
    matrix_t *out;

    if (!values)
        return NULL;

    count = rows * cols;
    tmp = calloc(count ? count : 1u, sizeof(*tmp));
    if (!tmp)
        return NULL;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_d(values[i]);

    out = mat_create(rows, cols, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
    return out;
}

matrix_t *test_mat_create_qf(size_t rows, size_t cols, const qfloat_t *values)
{
    size_t count, i;
    number_t *tmp;
    matrix_t *out;

    if (!values)
        return NULL;

    count = rows * cols;
    tmp = calloc(count ? count : 1u, sizeof(*tmp));
    if (!tmp)
        return NULL;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_mp_real(values[i]);

    out = mat_create(rows, cols, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
    return out;
}

matrix_t *test_mat_create_qc(size_t rows, size_t cols, const qcomplex_t *values)
{
    size_t count, i;
    number_t *tmp;
    matrix_t *out;

    if (!values)
        return NULL;

    count = rows * cols;
    tmp = calloc(count ? count : 1u, sizeof(*tmp));
    if (!tmp)
        return NULL;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_complex(values[i]);

    out = mat_create(rows, cols, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
    return out;
}

typedef struct {
    char labels[8][32];
    size_t count;
} precision_set_t;

static int precision_set_contains(const precision_set_t *set, const char *label_text)
{
    for (size_t i = 0; i < set->count; ++i)
        if (strcmp(set->labels[i], label_text) == 0)
            return 1;
    return 0;
}

static void precision_set_add(precision_set_t *set, const char *label_text)
{
    if (!set || !label_text || !*label_text || precision_set_contains(set, label_text))
        return;
    if (set->count >= (sizeof(set->labels) / sizeof(set->labels[0])))
        return;
    snprintf(set->labels[set->count], sizeof(set->labels[0]), "%.31s", label_text);
    set->count++;
}

static const char *number_precision_label(number_t z, char *buf, size_t buf_size)
{
    number_kind_t kind = number_kind_value(&z);
    size_t bits = num_get_prec_bits(z);

    switch (kind) {
    case NUMBER_DOUBLE:
        return "double";
    case NUMBER_QFLOAT:
        return "qfloat";
    case NUMBER_QCOMPLEX:
        return "qcomplex";
    case NUMBER_MFLOAT:
        snprintf(buf, buf_size, "mfloat-%zu", bits ? bits : num_get_default_prec_bits());
        return buf;
    case NUMBER_COMPLEX:
        snprintf(buf, buf_size, "complex-%zu", bits ? bits : num_get_default_prec_bits());
        return buf;
    case NUMBER_MINT:
    case NUMBER_MRATIONAL:
        return "exact";
    default:
        return "number";
    }
}

static int number_display_is_truncated(number_t z)
{
    size_t display_digits = 16u;
    size_t prec_digits = num_get_prec_digits(z);
    char *full = num_to_string(z);
    int truncated = 0;

    if (prec_digits > display_digits)
        truncated = 1;
    if (full && strlen(full) > 24u)
        truncated = 1;
    free(full);
    return truncated;
}

static void format_compact_number(number_t z, char *buf, size_t buf_size)
{
    number_t zr = num_real_part(z);
    number_t zi = num_imag_part(z);
    int ellipsis = number_display_is_truncated(z);

    if (num_is_real(z)) {
        snprintf(buf, buf_size, "%.16g%s", num_to_double(zr), ellipsis ? "..." : "");
    } else {
        number_t abs_zi = num_abs(zi);
        double re = num_to_double(zr);
        double im = num_to_double(abs_zi);
        const char *sign = (num_get_sign(zi) < 0) ? "-" : "+";

        snprintf(buf, buf_size, "%.16g %s %.16gi%s",
                 re, sign, im, ellipsis ? "..." : "");
        num_destroy(&abs_zi);
    }

    num_destroy(&zi);
    num_destroy(&zr);
}

static void print_mnum_raw(const char *label, matrix_t *A)
{
    size_t rows;
    size_t cols;
    size_t *w;
    precision_set_t precision_set = {{{0}}, 0};
    char precision_buf[64];

    if (!A) {
        printf(C_YELLOW "%s = <null>\n" C_RESET, label);
        return;
    }

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    w = calloc(cols ? cols : 1, sizeof(size_t));
    if (!w) {
        char *s = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);
        if (!s) {
            printf(C_YELLOW "%s = <null>\n" C_RESET, label);
            return;
        }
        printf(C_YELLOW "%s = \n%s\n" C_RESET, label, s);
        free(s);
        return;
    }

    printf(C_YELLOW "%s = " C_CYAN "(" C_RESET "\n", label);

    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            number_t z = mat_get_num(A, i, j);
            char buf[256];
            const char *plabel = number_precision_label(z, precision_buf, sizeof(precision_buf));

            format_compact_number(z, buf, sizeof(buf));
            precision_set_add(&precision_set, plabel);

            if (strlen(buf) > w[j])
                w[j] = strlen(buf);

            num_destroy(&z);
        }
    }

    if (precision_set.count > 0) {
        printf(C_GREY "  working precision: " C_RESET);
        for (size_t i = 0; i < precision_set.count; ++i) {
            if (i)
                printf(C_GREY ", " C_RESET);
            printf(C_WHITE "%s" C_RESET, precision_set.labels[i]);
        }
        printf("\n");
    }

    for (size_t i = 0; i < rows; i++) {
        printf("  ");
        for (size_t j = 0; j < cols; j++) {
            number_t z = mat_get_num(A, i, j);
            char buf[256];

            format_compact_number(z, buf, sizeof(buf));

            printf(" %*s", (int)w[j], buf);

            num_destroy(&z);
        }
        printf("\n");
    }

    printf(C_CYAN ")" C_RESET "\n");
    free(w);
}

static int is_primary_matrix_label(const char *label)
{
    static const char *derived_prefixes[] = {
        "exp(", "sin(", "cos(", "tan(", "sinh(", "cosh(", "tanh(",
        "sqrt(", "log(", "asin(", "acos(", "atan(", "asinh(",
        "acosh(", "atanh(", "erf(", "erfc(", "transpose(", "conj(",
        "eigenvectors", "V ", NULL};

    if (!label || !label[0])
        return 0;
    if (label[0] < 'A' || label[0] > 'Z')
        return 0;

    for (size_t i = 0; derived_prefixes[i]; i++)
        if (strncmp(label, derived_prefixes[i], strlen(derived_prefixes[i])) == 0)
            return 0;

    size_t token_len = 0;
    while ((label[token_len] >= 'A' && label[token_len] <= 'Z') ||
           (label[token_len] >= 'a' && label[token_len] <= 'z') ||
           (label[token_len] >= '0' && label[token_len] <= '9'))
        token_len++;

    if (token_len == 0)
        return 0;

    while (label[token_len] == ' ')
        token_len++;

    if (label[token_len] == '\0' || label[token_len] == '(')
        return 1;

    if ((label[token_len] >= 'A' && label[token_len] <= 'Z') ||
        (label[token_len] >= 'a' && label[token_len] <= 'z'))
        return 1;

    if (label[0] == 'I' && label[token_len] == '+' && label[token_len + 1] != '\0')
        return 1;

    return 0;
}

static void remember_matrix_input(const char *label, matrix_t *A)
{
    if (!is_primary_matrix_label(label))
        return;

    snprintf(current_matrix_input_label, sizeof(current_matrix_input_label), "%s", label);
    mat_free(current_matrix_input);
    current_matrix_input = clone_matrix_snapshot(A);
}

void clear_matrix_input_context(void)
{
    current_matrix_input_label[0] = '\0';
    mat_free(current_matrix_input);
    current_matrix_input = NULL;
}

static void format_matrix_label(const char *label, char *out, size_t out_size)
{
    if (is_primary_matrix_label(label))
        snprintf(out, out_size, "input %s", label);
    else
        snprintf(out, out_size, "%s", label);
}

void d_to_coloured_string(double x, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else if (x >= tol)
        snprintf(out, out_size, C_RED "%.16g" C_RESET, x);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size)
{
    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%q", x);
    if (strcmp(buf, "0") == 0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_YELLOW "%s" C_RESET, buf);
}

void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size)
{
    char re[128], im[128];
    qf_sprintf(re, sizeof(re), "%q", qc_real(z));
    qf_sprintf(im, sizeof(im), "%q", qf_abs(qc_imag(z)));
    const char *sign = (qc_imag(z).hi >= 0.0) ? "+" : "-";
    snprintf(out, out_size,
             C_GREEN "%s" C_RESET " " C_WHITE "%s" C_RESET " " C_MAGENTA "%si" C_RESET,
             re, sign, im);
}

void print_complex(const char *label, qcomplex_t z)
{
    char buf[512];
    qc_to_coloured_string(z, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

void print_mp_real(const char *label, qfloat_t x)
{
    char buf[512];
    qf_to_coloured_string(x, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

static size_t visible_string_width(const char *s)
{
    size_t width = 0;

    while (s && *s) {
        if (s[0] == '\033' && s[1] == '[') {
            s += 2;
            while (*s && *s != 'm')
                s++;
            if (*s == 'm')
                s++;
            continue;
        }
        width++;
        s++;
    }

    return width;
}

static char *dup_trimmed_token(const char *start, size_t len)
{
    while (len > 0 && (*start == ' ' || *start == '\t')) {
        start++;
        len--;
    }
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t'))
        len--;

    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static int binding_contains(char **bindings, size_t nb, const char *token)
{
    for (size_t i = 0; i < nb; ++i)
        if (strcmp(bindings[i], token) == 0)
            return 1;
    return 0;
}

static void append_binding(char ***bindings,
                           size_t *nbindings,
                           size_t *capbindings,
                           char *token)
{
    if (!token || !*token) {
        free(token);
        return;
    }

    if (binding_contains(*bindings, *nbindings, token)) {
        free(token);
        return;
    }

    if (*nbindings == *capbindings) {
        size_t new_cap = *capbindings ? (*capbindings * 2) : 8;
        char **grown = realloc(*bindings, new_cap * sizeof(**bindings));
        if (!grown) {
            free(token);
            return;
        }
        *bindings = grown;
        *capbindings = new_cap;
    }

    (*bindings)[(*nbindings)++] = token;
}

static void collect_bindings(char ***var_bindings,
                             size_t *nvar_bindings,
                             size_t *capvar_bindings,
                             char ***const_bindings,
                             size_t *nconst_bindings,
                             size_t *capconst_bindings,
                             const char *binding_text)
{
    const char *p = binding_text;
    int in_constants = 0;

    while (p && *p) {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == ';') {
            in_constants = 1;
            p++;
            continue;
        }
        if (!*p)
            break;

        const char *start = p;
        while (*p && *p != ',' && *p != ';')
            p++;

        char *token = dup_trimmed_token(start, (size_t)(p - start));
        if (in_constants)
            append_binding(const_bindings, nconst_bindings, capconst_bindings, token);
        else
            append_binding(var_bindings, nvar_bindings, capvar_bindings, token);
    }
}

static void collect_dval_bindings(const dval_t *dv,
                                  char ***var_bindings,
                                  size_t *nvar_bindings,
                                  size_t *capvar_bindings,
                                  char ***const_bindings,
                                  size_t *nconst_bindings,
                                  size_t *capconst_bindings,
                                  const char *binding_text)
{
    if (dv && dv_is_named_const(dv) &&
        binding_text && *binding_text && !strchr(binding_text, ';')) {
        append_binding(const_bindings, nconst_bindings, capconst_bindings,
                       strdup(binding_text));
        return;
    }

    collect_bindings(var_bindings, nvar_bindings, capvar_bindings,
                     const_bindings, nconst_bindings, capconst_bindings,
                     binding_text);
}

static char *join_binding_list(char **bindings, size_t nbindings)
{
    size_t total = 1;

    for (size_t i = 0; i < nbindings; ++i)
        total += strlen(bindings[i]) + (i ? 2 : 0);

    char *out = malloc(total);
    if (!out)
        return NULL;
    out[0] = '\0';

    for (size_t i = 0; i < nbindings; ++i) {
        if (i)
            strcat(out, ", ");
        strcat(out, bindings[i]);
    }

    return out;
}

static int split_binding_token(const char *binding, char **name_out, char **value_out)
{
    const char *eq;
    char *name;
    char *value;

    *name_out = NULL;
    *value_out = NULL;
    if (!binding)
        return -1;

    eq = strstr(binding, "=");
    if (!eq)
        return -1;

    name = strndup(binding, (size_t)(eq - binding));
    value = strdup(eq + 1);
    if (!name || !value) {
        free(name);
        free(value);
        return -1;
    }

    while (*name == ' ' || *name == '\t')
        memmove(name, name + 1, strlen(name));
    while (*value == ' ' || *value == '\t')
        memmove(value, value + 1, strlen(value));

    for (size_t len = strlen(name); len > 0; --len) {
        if (name[len - 1] == ' ' || name[len - 1] == '\t')
            name[len - 1] = '\0';
        else
            break;
    }
    for (size_t len = strlen(value); len > 0; --len) {
        if (value[len - 1] == ' ' || value[len - 1] == '\t')
            value[len - 1] = '\0';
        else
            break;
    }

    *name_out = name;
    *value_out = value;
    return 0;
}

static int has_long_binding(char **bindings, size_t nbindings, size_t threshold)
{
    for (size_t i = 0; i < nbindings; ++i) {
        if (strlen(bindings[i]) > threshold)
            return 1;
    }
    return 0;
}

static char *join_bindings(char **var_bindings,
                           size_t nvar_bindings,
                           char **const_bindings,
                           size_t nconst_bindings)
{
    if (nvar_bindings == 0) {
        if (!has_long_binding(const_bindings, nconst_bindings, 16))
            return strdup("");
        return join_binding_list(const_bindings, nconst_bindings);
    }

    char *vars = join_binding_list(var_bindings, nvar_bindings);
    char *consts = join_binding_list(const_bindings, nconst_bindings);
    char *out;
    size_t total;

    if (!vars || !consts) {
        free(vars);
        free(consts);
        return NULL;
    }

    total = strlen(vars) + strlen(consts) + 4;
    out = malloc(total);
    if (!out) {
        free(vars);
        free(consts);
        return NULL;
    }

    out[0] = '\0';
    if (*vars)
        strcat(out, vars);
    if (*consts) {
        if (*vars)
            strcat(out, "; ");
        strcat(out, consts);
    }

    free(vars);
    free(consts);
    return out;
}

static int split_dval_repr(const dval_t *dv, char **expr_out, char **bindings_out)
{
    char *tmp;
    char *body;
    char *sep;
    size_t len;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (!dv) {
        *expr_out = strdup("NULL");
        *bindings_out = strdup("");
        return *expr_out && *bindings_out ? 0 : -1;
    }

    tmp = dv_to_string(dv, style_EXPRESSION);
    if (!tmp)
        return -1;

    body = tmp;
    len = strlen(tmp);
    if (len >= 4 && tmp[0] == '{' && tmp[1] == ' ' &&
        tmp[len - 2] == ' ' && tmp[len - 1] == '}') {
        body = tmp + 2;
        tmp[len - 2] = '\0';
    }

    sep = strstr(body, " | ");
    if (sep) {
        *sep = '\0';
        *expr_out = strdup(body);
        *bindings_out = strdup(sep + 3);
    } else {
        *expr_out = strdup(body);
        *bindings_out = strdup("");
    }

    free(tmp);
    return *expr_out && *bindings_out ? 0 : -1;
}

static void pretty_dval_expr(char **expr_io,
                             char **const_bindings,
                             size_t nconst_bindings)
{
    char *expr;

    if (!expr_io || !*expr_io)
        return;

    expr = *expr_io;
    for (size_t i = 0; i < nconst_bindings; ++i) {
        char *name = NULL;
        char *value = NULL;

        if (split_binding_token(const_bindings[i], &name, &value) != 0)
            continue;

        if (strlen(const_bindings[i]) > 16 && strcmp(expr, value) == 0) {
            char *replacement = strdup(name);
            if (replacement) {
                free(*expr_io);
                *expr_io = replacement;
            }
            free(name);
            free(value);
            return;
        }

        free(name);
        free(value);
    }
}

static void print_md_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            double v;
            char buf[256];
            mat_get(A, i, j, &v);
            d_to_coloured_string(v, buf, sizeof(buf));
            size_t len = visible_string_width(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
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

void print_md(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_md_raw(display_label, A);
}

static void print_mqf_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            qfloat_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            size_t len = visible_string_width(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
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

void print_mqf(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_mqf_raw(display_label, A);
}

static void print_mqc_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            qcomplex_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qc_to_coloured_string(v, buf, sizeof(buf));
            size_t len = visible_string_width(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
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

void print_mqc(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_mqc_raw(display_label, A);
}

static void print_mdv_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "{ [" C_RESET "\n", label);

    char **exprs = calloc(rows * cols, sizeof(*exprs));
    char **var_bindings = NULL;
    char **const_bindings = NULL;
    size_t *w = calloc(cols ? cols : 1, sizeof(size_t));
    size_t nvar_bindings = 0, capvar_bindings = 0;
    size_t nconst_bindings = 0, capconst_bindings = 0;
    int ok = exprs && w;

    for (size_t i = 0; ok && i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            dval_t *v;
            char *expr = NULL;
            char *binding_text = NULL;
            char buf[2048];
            size_t idx = i * cols + j;

            mat_get(A, i, j, &v);
            if (split_dval_repr(v, &expr, &binding_text) != 0) {
                free(expr);
                free(binding_text);
                ok = 0;
                break;
            }

            exprs[idx] = expr;
            collect_dval_bindings(v,
                                  &var_bindings, &nvar_bindings, &capvar_bindings,
                                  &const_bindings, &nconst_bindings, &capconst_bindings,
                                  binding_text);
            pretty_dval_expr(&exprs[idx], const_bindings, nconst_bindings);
            snprintf(buf, sizeof(buf), C_WHITE "%s" C_RESET, exprs[idx]);
            if (visible_string_width(buf) > w[j])
                w[j] = visible_string_width(buf);
            free(binding_text);
            dv_free(v);
        }

    if (!ok) {
        printf("      " C_WHITE "<dval matrix>" C_RESET "\n");
        printf("    " C_CYAN "] }" C_RESET "\n");
    } else {
        char *joined = join_bindings(var_bindings, nvar_bindings,
                                     const_bindings, nconst_bindings);
        for (size_t i = 0; i < rows; i++)
        {
            printf("      ");
            for (size_t j = 0; j < cols; j++)
            {
                char buf[2048];
                size_t idx = i * cols + j;
                snprintf(buf, sizeof(buf), C_WHITE "%s" C_RESET, exprs[idx] ? exprs[idx] : "");
                printf(" %*s", (int)w[j], buf);
            }
            printf("\n");
        }

        if (joined && *joined)
            printf("    " C_CYAN "]" C_RESET " | %s " C_CYAN "}" C_RESET "\n", joined);
        else
            printf("    " C_CYAN "] }" C_RESET "\n");
        free(joined);
    }

    for (size_t i = 0; i < rows * cols; ++i)
        free(exprs[i]);
    for (size_t i = 0; i < nvar_bindings; ++i)
        free(var_bindings[i]);
    for (size_t i = 0; i < nconst_bindings; ++i)
        free(const_bindings[i]);
    free(var_bindings);
    free(const_bindings);
    free(exprs);
    free(w);
}

void print_mnum(const char *label, matrix_t *A)
{
    char display_label[160];

    if (!A) {
        printf(C_YELLOW "%s = <null>\n" C_RESET, label);
        fflush(stdout);
        return;
    }

    format_matrix_label(label, display_label, sizeof(display_label));
    print_mnum_raw(display_label, A);
    remember_matrix_input(label, A);
    fflush(stdout);
}

void print_matrix_working_precision_line(const char *label, const matrix_t *A)
{
    precision_set_t precision_set = {{{0}}, 0};
    char precision_buf[64];

    if (!A || mat_typeof(A) != MAT_TYPE_NUMBER)
        return;

    for (size_t i = 0; i < mat_get_row_count(A); ++i) {
        for (size_t j = 0; j < mat_get_col_count(A); ++j) {
            number_t z = mat_get_num(A, i, j);
            const char *plabel = number_precision_label(z, precision_buf, sizeof(precision_buf));
            precision_set_add(&precision_set, plabel);
            num_destroy(&z);
        }
    }

    if (precision_set.count == 0)
        return;

    if (label && *label)
        printf("    working precision (%s) = ", label);
    else
        printf("    working precision = ");

    for (size_t i = 0; i < precision_set.count; ++i) {
        if (i)
            printf(", ");
        printf("%s", precision_set.labels[i]);
    }
    printf("\n");
}

void print_mdv(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_mdv_raw(display_label, A);
}

void print_current_input_matrix(void)
{
    if (!current_matrix_input || current_matrix_input_label[0] == '\0')
        return;

    switch (mat_typeof(current_matrix_input))
    {
    case MAT_TYPE_NUMBER:
        print_mnum_raw("input matrix", current_matrix_input);
        break;
    case MAT_TYPE_DVAL:
        print_mdv_raw("input matrix", current_matrix_input);
        break;
    default:
        break;
    }
}

void check_d(const char *label, double got, double expected, double tol)
{
    double err = fabs(got - expected);
    int ok = err < tol;

    if (!ok)
        test_mark_failure(__FILE__, __LINE__, label ? label : "check_d failure");

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    char gbuf[256], ebuf[256];
    d_to_coloured_string(got, gbuf, sizeof(gbuf));
    d_to_coloured_string(expected, ebuf, sizeof(ebuf));

    printf("    got      = %s\n", gbuf);
    printf("    expected = %s\n", ebuf);
    printf("    error    = %.16g\n", err);
}

void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol)
{
    qfloat_t diff = qf_abs(qf_sub(got, expected));
    double err = diff.hi;
    int ok = err < tol;

    if (!ok)
        test_mark_failure(__FILE__, __LINE__, label ? label : "check_qf_val failure");

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    print_mp_real("got      ", got);
    print_mp_real("expected ", expected);
    printf("    error    = %.16g\n", err);
}

void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol)
{
    double got_re = qf_to_double(qc_real(got));
    double got_im = qf_to_double(qc_imag(got));
    double exp_re = qf_to_double(qc_real(expected));
    double exp_im = qf_to_double(qc_imag(expected));
    int both_nan = isnan(got_re) && isnan(got_im) && isnan(exp_re) && isnan(exp_im);
    double err = both_nan ? 0.0 : qf_to_double(qc_abs(qc_sub(got, expected)));
    int ok = both_nan || err < tol;

    if (!ok)
        test_mark_failure(__FILE__, __LINE__, label ? label : "check_qc_val failure");

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    print_complex("got      ", got);
    print_complex("expected ", expected);
    printf("    error    = %.16g\n", err);
}

void check_bool(const char *label, int cond)
{
    if (!cond)
        test_mark_failure(__FILE__, __LINE__, label ? label : "check_bool failure");

    printf(cond ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
                : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);
}
