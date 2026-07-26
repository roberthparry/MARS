#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression/expr_stringout.h"
#include "expression/expr_stringout_internal.h"

bool expr_set_number_scientific_local(bool scientific);
int expr_set_number_precision_local(int precision);

static void equ_emit_function_name(sbuf_t *buffer, const char *name)
{
    emit_name_func(buffer, name);
}

static void equ_emit_function_parameter_list(sbuf_t *buffer,
                                             const varlist_t *variables,
                                             const varlist_t *constants)
{
    if (variables->count == 0u && constants->count == 0u) {
        sbuf_puts(buffer, "void");
        return;
    }

    for (size_t i = 0u; i < variables->count; ++i) {
        if (i > 0u)
            sbuf_puts(buffer, ", ");
        equ_emit_function_name(
            buffer, expr_name_or_default(variables->vars[i], "x"));
    }
    for (size_t i = 0u; i < constants->count; ++i) {
        if (variables->count > 0u || i > 0u)
            sbuf_puts(buffer, ", ");
        sbuf_puts(buffer, "const ");
        equ_emit_function_name(buffer, constants->vars[i]->name);
    }
}

static void equ_emit_function_argument_list(sbuf_t *buffer,
                                            const varlist_t *variables,
                                            const varlist_t *constants)
{
    for (size_t i = 0u; i < variables->count; ++i) {
        if (i > 0u)
            sbuf_puts(buffer, ", ");
        equ_emit_function_name(
            buffer, expr_name_or_default(variables->vars[i], "x"));
    }
    for (size_t i = 0u; i < constants->count; ++i) {
        if (variables->count > 0u || i > 0u)
            sbuf_puts(buffer, ", ");
        equ_emit_function_name(buffer, constants->vars[i]->name);
    }
}

static void equ_emit_unknown_variable_hint(sbuf_t *buffer,
                                           const varlist_t *variables)
{
    bool emitted = false;

    for (size_t i = 0u; i < variables->count; ++i) {
        number_t value = expr_get_val(variables->vars[i]);
        bool unknown = num_is_nan(value);

        num_destroy(&value);
        if (!unknown)
            continue;

        if (!emitted)
            sbuf_puts(buffer, "// ");
        else
            sbuf_puts(buffer, ", ");
        equ_emit_function_name(
            buffer, expr_name_or_default(variables->vars[i], "x"));
        sbuf_puts(buffer, " = ?");
        emitted = true;
    }

    if (emitted)
        sbuf_putc(buffer, '\n');
}

static char *equ_function_expression_dup(const expr_t *expression)
{
    sbuf_t buffer;
    char *text;

    sbuf_init(&buffer);
    emit_func_display(expression, &buffer, PREC_LOWEST);
    text = sbuf_take_c_string(&buffer);
    sbuf_free(&buffer);
    return text;
}

static bool equ_is_function_break_operator(char operation,
                                           bool multiplicative)
{
    return multiplicative
        ? operation == '*' || operation == '/'
        : operation == '+' || operation == '-';
}

static const char *equ_find_top_level_function_separator(
    const char *text,
    bool multiplicative)
{
    int parentheses = 0;
    int brackets = 0;

    if (!text)
        return NULL;

    for (const char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '(')
            parentheses++;
        else if (*cursor == ')' && parentheses > 0)
            parentheses--;
        else if (*cursor == '[')
            brackets++;
        else if (*cursor == ']' && brackets > 0)
            brackets--;
        else if (parentheses == 0 && brackets == 0 &&
                 *cursor == ' ' &&
                 equ_is_function_break_operator(cursor[1], multiplicative) &&
                 cursor[2] == ' ')
            return cursor;
    }

    return NULL;
}

static void equ_emit_function_text_range(sbuf_t *buffer,
                                         const char *start,
                                         const char *end)
{
    for (const char *cursor = start; cursor && cursor < end; ++cursor)
        sbuf_putc(buffer, *cursor);
}

static void equ_emit_aligned_function_expression(sbuf_t *buffer,
                                                 const char *text)
{
    static const size_t line_limit = 88u;
    const char *term = text ? text : "0";
    bool multiplicative =
        equ_find_top_level_function_separator(term, false) == NULL;
    const char *separator = equ_find_top_level_function_separator(
        term, multiplicative);
    const char *end = separator ? separator : term + strlen(term);
    size_t column;

    if (*term == '-') {
        sbuf_puts(buffer, "        - ");
        term++;
    } else {
        sbuf_puts(buffer, "          ");
    }
    equ_emit_function_text_range(buffer, term, end);
    column = 10u + (size_t)(end - term);

    while (separator) {
        char operation = separator[1];
        size_t term_length;

        term = separator + 3;
        separator = equ_find_top_level_function_separator(
            term, multiplicative);
        end = separator ? separator : term + strlen(term);
        term_length = (size_t)(end - term);
        if (column + 3u + term_length > line_limit) {
            sbuf_puts(buffer, "\n        ");
            sbuf_putc(buffer, operation);
            sbuf_putc(buffer, ' ');
            column = 10u + term_length;
        } else {
            sbuf_putc(buffer, ' ');
            sbuf_putc(buffer, operation);
            sbuf_putc(buffer, ' ');
            column += 3u + term_length;
        }
        equ_emit_function_text_range(buffer, term, end);
    }

    sbuf_putc(buffer, '\n');
}

static void equ_emit_function_equation_return(sbuf_t *buffer,
                                              const equation_t *equation)
{
    static const size_t compact_line_limit = 88u;
    char *lhs = equ_function_expression_dup(equ_lhs(equation));
    char *rhs = equ_function_expression_dup(equ_rhs(equation));
    char *relation = NULL;
    size_t relation_length;
    size_t compact_length;

    if (!lhs || !rhs) {
        free(rhs);
        free(lhs);
        sbuf_puts(buffer, "    return equation(0 = 0);\n");
        return;
    }

    relation_length = strlen(lhs) + strlen(" = ") + strlen(rhs);
    relation = malloc(relation_length + 1u);
    if (!relation) {
        free(rhs);
        free(lhs);
        sbuf_puts(buffer, "    return equation(0 = 0);\n");
        return;
    }
    snprintf(relation, relation_length + 1u, "%s = %s", lhs, rhs);

    compact_length = strlen("    return equation();") + relation_length;
    if (compact_length <= compact_line_limit) {
        sbuf_puts(buffer, "    return equation(");
        sbuf_puts(buffer, relation);
        sbuf_puts(buffer, ");\n");
    } else {
        sbuf_puts(buffer, "    return equation(\n");
        equ_emit_aligned_function_expression(buffer, relation);
        sbuf_puts(buffer, "    );\n");
    }

    free(relation);
    free(rhs);
    free(lhs);
}

static int equ_append_padding(string_t *out, int count)
{
    for (int i = 0; i < count; ++i) {
        if (string_append_char(out, ' ') != 0)
            return -1;
    }
    return 0;
}

static style_t equ_format_style(const string_format_spec_t *spec,
                                string_format_result_t *result)
{
    if (!spec || !result)
        return style_EXPRESSION;

    switch (spec->trailing_modifier) {
        case 'u':
        case 'U':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_UNBOUND;
        case 't':
        case 'T':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_TEX;
        case 'f':
        case 'F':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_FUNCTION;
        default:
            return style_EXPRESSION;
    }
}

static string_format_result_t equ_format_callback(string_t *out,
                                                  const string_format_spec_t *spec,
                                                  va_list ap,
                                                  void *user)
{
    bool left;
    bool old_scientific;
    bool scientific;
    int width;
    int old_precision;
    int precision;
    int pad;
    size_t text_len;
    const equation_t *equation;
    string_t *text;
    style_t style;
    string_format_result_t result = STRING_FORMAT_HANDLED;

    (void)user;

    if (!out || !spec || (spec->conversion != 'n' && spec->conversion != 'N'))
        return STRING_FORMAT_UNHANDLED;

    width = spec->width;
    left = spec->flag_left;
    if (spec->width_from_argument) {
        width = va_arg(ap, int);
        if (width < 0) {
            left = true;
            width = -width;
        }
    }
    precision = spec->precision;
    if (spec->precision_from_argument) {
        precision = va_arg(ap, int);
        if (precision < 0)
            precision = -1;
    }

    style = equ_format_style(spec, &result);
    scientific = spec->conversion == 'N';
    equation = va_arg(ap, const equation_t *);
    old_scientific = expr_set_number_scientific_local(scientific);
    old_precision = expr_set_number_precision_local(precision);
    text = equ_to_text(equation, style);
    expr_set_number_precision_local(old_precision);
    expr_set_number_scientific_local(old_scientific);
    if (!text)
        return STRING_FORMAT_ERROR;

    text_len = string_length(text);
    pad = width > (int)text_len ? width - (int)text_len : 0;

    if (!left && equ_append_padding(out, pad) != 0)
        goto fail;
    if (string_append_string(out, text) != 0)
        goto fail;
    if (left && equ_append_padding(out, pad) != 0)
        goto fail;

    string_free(text);
    return result;

fail:
    string_free(text);
    return STRING_FORMAT_ERROR;
}

static string_t *equ_extract_binding_section(string_t *expr_text)
{
    const char *raw = expr_text ? string_c_str(expr_text) : NULL;
    const char *start;
    const char *end;
    char *copy;
    string_t *out;
    size_t len;

    if (!raw)
        return NULL;

    start = strchr(raw, '|');
    end = strrchr(raw, '}');
    if (!start || !end || start >= end)
        return NULL;

    ++start;
    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n'))
        ++start;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n'))
        --end;

    len = (size_t)(end - start);
    if (len == 0u)
        return NULL;

    copy = malloc(len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, len);
    copy[len] = '\0';
    out = string_new_with(copy);
    free(copy);
    return out;
}

static string_t *equ_binding_section(const equation_t *equation)
{
    expr_t *combined;
    string_t *combined_text;
    string_t *bindings;

    combined = expr_add(equ_lhs(equation), equ_rhs(equation));
    if (!combined)
        return NULL;

    combined_text = expr_to_text(combined, style_EXPRESSION);
    expr_free(combined);
    if (!combined_text)
        return NULL;

    bindings = equ_extract_binding_section(combined_text);
    string_free(combined_text);
    return bindings;
}

static string_t *equ_to_text_expression(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equ_lhs(equation), style_UNBOUND);
    string_t *rhs = expr_to_text(equ_rhs(equation), style_UNBOUND);
    string_t *bindings = equ_binding_section(equation);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = bindings ? string_sprintf("{ %S = %S | %S }", lhs, rhs, bindings)
                   : string_sprintf("{ %S = %S }", lhs, rhs);

cleanup:
    string_free(bindings);
    string_free(rhs);
    string_free(lhs);
    return out;
}

static string_t *equ_to_text_unbound(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equ_lhs(equation), style_UNBOUND);
    string_t *rhs = expr_to_text(equ_rhs(equation), style_UNBOUND);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = string_sprintf("%S = %S", lhs, rhs);

cleanup:
    string_free(rhs);
    string_free(lhs);
    return out;
}

static string_t *equ_to_text_tex(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equ_lhs(equation), style_TEX);
    string_t *rhs = expr_to_text(equ_rhs(equation), style_TEX);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = string_sprintf("%S = %S", lhs, rhs);

cleanup:
    string_free(rhs);
    string_free(lhs);
    return out;
}

static string_t *equ_to_text_function(const equation_t *equation)
{
    sbuf_t buffer;
    autoname_table_t names;
    varlist_t variables;
    varlist_t constants;
    string_t *out;

    sbuf_init(&buffer);
    autoname_init(&names);
    assign_unnamed_vars_dfs((expr_t *)equ_lhs(equation), &names);
    assign_unnamed_vars_dfs((expr_t *)equ_rhs(equation), &names);

    varlist_init(&variables);
    find_vars_dfs(equ_lhs(equation), &variables);
    find_vars_dfs(equ_rhs(equation), &variables);

    varlist_init(&constants);
    find_explicit_named_consts_dfs(equ_lhs(equation), &constants);
    find_explicit_named_consts_dfs(equ_rhs(equation), &constants);
    find_named_consts_dfs(equ_lhs(equation), &constants);
    find_named_consts_dfs(equ_rhs(equation), &constants);

    sbuf_puts(&buffer, "equation equ(");
    equ_emit_function_parameter_list(&buffer, &variables, &constants);
    sbuf_puts(&buffer, ") {\n");
    equ_emit_function_equation_return(&buffer, equation);
    sbuf_puts(&buffer, "}\n\n");

    equ_emit_unknown_variable_hint(&buffer, &variables);
    sbuf_puts(&buffer, "output(equ(");
    equ_emit_function_argument_list(&buffer, &variables, &constants);
    sbuf_puts(&buffer, ").solve());");

    out = sbuf_to_string(&buffer);
    sbuf_free(&buffer);
    free(constants.vars);
    free(variables.vars);
    autoname_restore(&names);
    return out;
}

string_t *equ_to_text(const equation_t *equation, style_t style)
{
    if (!equation)
        return string_new_with("NULL");

    if (style == style_TEX)
        return equ_to_text_tex(equation);
    if (style == style_UNBOUND)
        return equ_to_text_unbound(equation);
    if (style == style_FUNCTION)
        return equ_to_text_function(equation);

    return equ_to_text_expression(equation);
}

string_t *equ_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt, ap, equ_format_callback, NULL);
}

string_t *equ_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *text;

    va_start(ap, fmt);
    text = equ_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int equ_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;
    string_t *text;
    size_t len;

    va_start(ap, fmt);
    text = equ_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    len = string_length(text);
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    n = len <= (size_t)INT_MAX ? (int)len : -1;
    string_free(text);
    return n;
}

int equ_printf(const char *fmt, ...)
{
    va_list ap;
    int written;
    string_t *text;

    va_start(ap, fmt);
    text = equ_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

void equ_print(const equation_t *equation)
{
    if (equ_printf("%n\n", equation) < 0)
        string_printf("NULL\n");
}

bool equ_serialize(const equation_t *equation,
                   string_t **out_type,
                   string_t **out_encoding,
                   void **out_data,
                   size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *payload = NULL;

    if (!equation || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    text = equ_to_text(equation, style_EXPRESSION);
    if (!text)
        return false;

    payload = malloc(string_byte_length(text));
    if (!payload) {
        string_free(text);
        return false;
    }
    memcpy(payload, string_c_str(text), string_byte_length(text));

    type = string_new_with("equation_t");
    encoding = string_new_with("mars/equation");
    if (!type || !encoding) {
        free(payload);
        string_free(text);
        string_free(type);
        string_free(encoding);
        return false;
    }

    *out_type = type;
    *out_encoding = encoding;
    *out_data = payload;
    *out_len = string_byte_length(text);
    string_free(text);
    return true;
}

equation_t *equ_deserialise(const void *data,
                            size_t len,
                            const string_t *type,
                            const string_t *encoding)
{
    string_t *text;
    equation_t *equation;

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "equation_t") != 0 ||
        strcmp(string_c_str(encoding), "mars/equation") != 0)
        return NULL;

    text = string_new();
    if (!text)
        return NULL;
    if (string_append_chars(text, (const char *)data, len) != 0) {
        string_free(text);
        return NULL;
    }
    equation = equ_from_text(text);
    string_free(text);
    return equation;
}
