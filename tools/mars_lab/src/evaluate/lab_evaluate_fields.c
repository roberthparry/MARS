/**
 * @file lab_evaluate_fields.c
 * @brief Decode Lab worker records and supply native Lab result cards.
 *
 * Labelled protocol records are selected by binary lookup. Repeated records and
 * multiline native renderings retain their order and contents. Binding arrays
 * come exclusively from native tab-separated records; no expression grammar or
 * mathematical rewriting is implemented here. Display aliases deliberately use
 * the same native strings, without Python precision or algebra transformations.
 */
#include <stdlib.h>
#include <string.h>

#include "lab_evaluate_internal.h"

typedef struct {
    const char *label;
    const char *key;
    bool multiline;
} field_t;

/* Sorted by native protocol label for logarithmic dispatch. */
static const field_t field_table[] = {
    {"algebraic_specialisation", "algebraic_specialisation", false},
    {"antiderivative", "antiderivative", false},
    {"antiderivative_TeX", "antiderivative_TeX", true},
    {"binding", "bindings", false},
    {"binding_expression", "binding_expression", false},
    {"binding_function_name", "binding_function_names", false},
    {"bound", "bound", false},
    {"bound_lower", "bound_lower", false},
    {"bound_upper", "bound_upper", false},
    {"bound_var", "bound_var", false},
    {"cols", "cols", false},
    {"complex", "complex", false},
    {"conditioned_expression", "conditioned_expression", true},
    {"d value", "derivative_value", false},
    {"d values", "derivative_values", false},
    {"derivation_TeX", "derivation_TeX", true},
    {"derivative", "derivative", false},
    {"derivative_TeX", "derivative_TeX", true},
    {"derivative_binding", "derivative_bindings", false},
    {"derivative_function", "derivative_function", true},
    {"diagnostic", "diagnostic", false},
    {"differentiable", "differentiable", false},
    {"dimensions", "dimensions", false},
    {"display_TeX", "display_TeX", true},
    {"display_solutions", "display_solutions", true},
    {"display_wrapped_TeX", "display_wrapped_TeX", true},
    {"equation", "equation", false},
    {"error", "error", false},
    {"evaluation_ready", "evaluation_ready", false},
    {"expression", "expression", false},
    {"expression_pretty", "expression_pretty", true},
    {"family_note", "family_note", false},
    {"function", "function", true},
    {"i value", "integral_value", false},
    {"input", "input", false},
    {"integral", "integral", false},
    {"integral_TeX", "integral_TeX", true},
    {"integral_binding", "integral_bindings", false},
    {"integral_function", "integral_function", true},
    {"interpretation_note", "interpretation_note", false},
    {"intervals", "intervals", false},
    {"iterations", "iterations", false},
    {"kind", "kind", false},
    {"max_intervals", "max_intervals", false},
    {"numeric", "numeric", true},
    {"operand", "operand", false},
    {"operation", "operation", false},
    {"operation_function", "operation_function", true},
    {"pretty", "pretty", true},
    {"problem", "problem", false},
    {"problem_TeX", "problem_TeX", true},
    {"residual", "residual", false},
    {"result", "result", false},
    {"root_expression", "root_expression", false},
    {"root_function", "root_function", true},
    {"root_tex", "root_TeX", false},
    {"root_value", "root_value", false},
    {"rows", "rows", false},
    {"search_note", "search_note", false},
    {"solutions", "solutions", true},
    {"solutions_TeX", "solutions_TeX", true},
    {"solutions_wrapped_TeX", "solutions_wrapped_TeX", true},
    {"solver", "solver", false},
    {"status", "status", false},
    {"steps", "steps", true},
    {"steps_TeX", "steps_TeX", true},
    {"symbolic", "symbolic", false},
    {"symbolic_TeX", "symbolic_TeX", true},
    {"symbolic_value", "symbolic_value", false},
    {"symmetry", "symmetry", false},
    {"tex", "tex", true},
    {"transform_identity_TeX", "transform_identity_TeX", false},
    {"unbound", "unbound", false},
    {"value", "value", false},
    {"value_note", "value_note", false},
    {"value_pretty", "value_pretty", true},
    {"value_tex", "value_tex", true},
    {"work_cap", "work_cap", false},
    {"work_units", "work_units", false},
};

/* Access a member without leaking the temporary string key. */
const json_t *lab_eval_get(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *value = name && object ? json_object_get(object, name) : NULL;
    string_free(name);
    return value;
}

/* Expose borrowed native UTF-8 text for immediate use. */
const char *lab_eval_text(const json_t *object, const char *key)
{
    const string_t *text = json_string_value(lab_eval_get(object, key));
    return text ? string_c_str(text) : "";
}

/* Consume a temporary JSON value after copying it into the response. */
bool lab_eval_put(json_t *object, const char *key, json_t *value)
{
    string_t *name = string_new_with(key);
    bool ok = object && name && value && json_object_set(object, name, value);
    string_free(name);
    json_free(value);
    return ok;
}

/* Copy native text into a response field. */
bool lab_eval_set(json_t *object, const char *key, const char *value)
{
    string_t *text = string_new_with(value ? value : "");
    bool ok = text && lab_eval_put(object, key, json_new_string(text));
    string_free(text);
    return ok;
}

static int lab_eval_compare_field(const void *key, const void *entry)
{
    return strcmp(key, ((const field_t *)entry)->label);
}

/* Format the native binding-envelope protocol without changing its algebra or literal numerical values. */
string_t *lab_eval_display_bindings(const string_t *output)
{
    string_cursor_t *cursor = output ? string_cursor_new(output) : NULL;
    string_t *result = cursor ? string_new() : NULL;
    bool binding = false, quoted = false, ok = result != NULL;
    while (ok && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned char byte = 0xff;
        string_cursor_peek_ascii(cursor, &byte);
        if (byte == '[')
            quoted = true;
        else if (byte == ']')
            quoted = false;
        if (!quoted && (byte == '|' || byte == '}' || byte == '\n'))
            binding = byte == '|';
        if (binding && !quoted &&
            (string_cursor_match(cursor, " = NAN,") || string_cursor_match(cursor, " = NAN;") ||
             string_cursor_match(cursor, " = NAN }"))) {
            string_cursor_consume(cursor, " = NAN");
            ok = string_append_cstr(result, " = ?") == 0;
        } else {
            ok = string_append_rune(result, rune) == 0;
            string_cursor_next(cursor);
        }
    }
    string_cursor_free(cursor);
    if (!ok) {
        string_free(result);
        return NULL;
    }
    return result;
}

static string_t **lab_eval_split_lines(const string_t *text, size_t *count)
{
    string_t *separator = string_new_with("\n");
    string_t **lines = text && separator ? string_split_string(text, separator, count) : NULL;
    string_free(separator);
    return lines;
}

static bool lab_eval_append_record(string_t **record, const string_t *value)
{
    bool repeated = *record != NULL;
    if (!*record)
        *record = string_new();
    return *record && (!repeated || !string_append_char(*record, '\n')) && !string_append_string(*record, value);
}

/* Decode only protocol labels, leaving mathematical contents untouched. */
json_t *lab_eval_fields(const string_t *output)
{
    json_t *fields = json_new_object();
    size_t count = 0u;
    string_t **lines = output ? lab_eval_split_lines(output, &count) : NULL;
    const field_t *continuation = NULL;
    string_t *records[sizeof(field_table) / sizeof(*field_table)] = {0};
    bool ok = fields && (!output || !string_byte_length(output) || lines);
    for (size_t i = 0u; ok && i < count; ++i) {
        string_t *line = lines[i];
        string_view_t view = string_view_all(line);
        size_t length = string_byte_length(line);
        if (!length && i + 1u == count)
            break;
        size_t boundary = 0u;
        unsigned char byte = 0u;
        while (boundary < length && string_view_peek_ascii(view, boundary, &byte) && byte != ' ' && byte != '\t' &&
               byte != '\r')
            ++boundary;
        /* Two historical native labels contain an internal space. */
        if (string_starts_with(line, "d values "))
            boundary = 8u;
        else if (string_starts_with(line, "d value ") || string_starts_with(line, "i value "))
            boundary = 7u;
        string_t *label = string_substr(line, 0u, boundary);
        const field_t *field = label && boundary < length ? bsearch(string_c_str(label), field_table,
                                                                    sizeof(field_table) / sizeof(*field_table),
                                                                    sizeof(*field_table), lab_eval_compare_field)
                                                          : NULL;
        string_free(label);
        if (field) {
            while (boundary < length && string_view_peek_ascii(view, boundary, &byte) && (byte == ' ' || byte == '\t'))
                ++boundary;
            size_t end = length;
            while (end > boundary && string_view_peek_ascii(view, end - 1u, &byte) &&
                   (byte == '\r' || byte == ' ' || byte == '\t'))
                --end;
            string_t *value = string_substr(line, boundary, end - boundary);
            ok = value && lab_eval_append_record(&records[field - field_table], value);
            string_free(value);
            continuation = field->multiline ? field : NULL;
        } else if (continuation) {
            ok = lab_eval_append_record(&records[continuation - field_table], line);
        }
    }
    string_split_free(lines, count);
    for (size_t i = 0u; i < sizeof(records) / sizeof(*records); ++i) {
        if (records[i]) {
            if (ok)
                ok = lab_eval_put(fields, field_table[i].key, json_new_string(records[i]));
            string_free(records[i]);
        }
    }
    if (!ok) {
        json_free(fields);
        fields = NULL;
    }
    return fields;
}

static void lab_eval_alias(json_t *fields, const char *destination, const char *source)
{
    /* set() copies before replacing, including when source equals destination. */
    lab_eval_set(fields, destination, lab_eval_text(fields, source));
}

static const char *lab_eval_first_field(const json_t *fields, const char *const keys[], size_t count)
{
    /* A bounded priority list, not a search through bindings or mathematical nodes. */
    for (size_t i = 0u; i < count; ++i) {
        if (*lab_eval_text(fields, keys[i]))
            return keys[i];
    }
    return keys[count - 1u];
}

static void lab_eval_display_pair(json_t *fields, const char *name, const char *source)
{
    string_t *full = string_sprintf("full_display_%s", name);
    string_t *display = string_sprintf("display_%s", name);
    if (full && display) {
        lab_eval_alias(fields, string_c_str(full), source);
        lab_eval_alias(fields, string_c_str(display), source);
    }
    string_free(full);
    string_free(display);
}

static bool lab_eval_unset_value(const char *value)
{
    return !*value || !strcmp(value, "?") || !strcmp(value, "NAN") || !strcmp(value, "nan") || !strcmp(value, "NaN") ||
           !strcmp(value, "(null)");
}

static void lab_eval_binding_array(json_t *fields, const char *source, const char *destination)
{
    const string_t *records = json_string_value(lab_eval_get(fields, source));
    json_t *values = json_new_array();
    size_t count = 0u;
    string_t **lines = records ? string_split(records, "\n", &count) : NULL;
    for (size_t i = 0u; i < count; ++i) {
        string_offset_t first = string_find(lines[i], "\t");
        if (first < 0)
            continue;
        string_t *kind = string_substr(lines[i], 0u, (size_t)first);
        string_t *tail = string_substr(lines[i], (size_t)first + 1u, string_byte_length(lines[i]) - (size_t)first - 1u);
        string_offset_t second = tail ? string_find(tail, "\t") : -1;
        if (second >= 0 && kind &&
            (!strcmp(string_c_str(kind), "variable") || !strcmp(string_c_str(kind), "constant"))) {
            string_t *name = string_substr(tail, 0u, (size_t)second);
            string_t *value = string_substr(tail, (size_t)second + 1u, string_byte_length(tail) - (size_t)second - 1u);
            if (name && value && string_byte_length(name)) {
                json_t *binding = json_new_object();
                lab_eval_set(binding, "kind", string_c_str(kind));
                lab_eval_set(binding, "name", string_c_str(name));
                lab_eval_set(binding, "value", string_c_str(value));
                lab_eval_set(binding, "display", lab_eval_unset_value(string_c_str(value)) ? "" : string_c_str(value));
                json_array_append(values, binding);
                json_free(binding);
            }
            string_free(name);
            string_free(value);
        }
        string_free(kind);
        string_free(tail);
    }
    string_split_free(lines, count);
    lab_eval_put(fields, destination, values);
}

static json_t *lab_eval_line_array(const json_t *fields, const char *key)
{
    const string_t *value = json_string_value(lab_eval_get(fields, key));
    size_t count = 0u;
    string_t **lines = value ? string_split(value, "\n", &count) : NULL;
    json_t *array = json_new_array();
    for (size_t i = 0u; i < count; ++i) {
        string_trim(lines[i]);
        if (string_byte_length(lines[i])) {
            json_t *line = json_new_string(lines[i]);
            json_array_append(array, line);
            json_free(line);
        }
    }
    string_split_free(lines, count);
    return array;
}

static void lab_eval_expression_fields(json_t *fields)
{
    const char *const expressions[] = {"conditioned_expression", "root_expression", "expression", "unbound"};
    const char *const renderings[] = {"transform_identity_TeX", "derivation_TeX", "root_TeX", "tex"};
    const char *const functions[] = {"root_function", "operation_function", "function"};
    lab_eval_display_pair(fields, "expression", lab_eval_first_field(fields, expressions, 4u));
    lab_eval_display_pair(fields, "TeX", lab_eval_first_field(fields, renderings, 4u));
    lab_eval_display_pair(fields, "function", lab_eval_first_field(fields, functions, 3u));
    lab_eval_display_pair(fields, "derivative_function", "derivative_function");
    lab_eval_display_pair(fields, "integral_function", "integral_function");
    lab_eval_alias(fields, "editor_expression", "input");
    if (*lab_eval_text(fields, "root_value"))
        lab_eval_alias(fields, "value", "root_value");
    if (*lab_eval_text(fields, "derivative_values"))
        lab_eval_alias(fields, "derivative_value", "derivative_values");
    /* An empty value is false in the UI, so no numerical card is displayed. */
    const char *values[] = {"value", "derivative_value", "integral_value"};
    for (size_t i = 0u; i < 3u; ++i) {
        if (lab_eval_unset_value(lab_eval_text(fields, values[i])))
            lab_eval_set(fields, values[i], "");
    }
    lab_eval_binding_array(fields, "derivative_bindings", "derivative_binding_values");
    lab_eval_binding_array(fields, "integral_bindings", "integral_binding_values");
    lab_eval_render_field(fields, "display_TeX", "svg", "render_error");
    lab_eval_render_field(fields, "derivative_TeX", "derivative_svg", "derivative_render_error");
    lab_eval_render_field(fields, "integral_TeX", "integral_svg", "integral_render_error");
}

static void lab_eval_matrix_fields(json_t *fields)
{
    if (!*lab_eval_text(fields, "result"))
        lab_eval_alias(fields, "result", "value");
    if (!*lab_eval_text(fields, "expression"))
        lab_eval_alias(fields, "expression", "result");
    if (!*lab_eval_text(fields, "expression_pretty"))
        lab_eval_alias(fields, "expression_pretty", "expression");
    lab_eval_alias(fields, "display_result", "result");
    lab_eval_alias(fields, "display_expression_pretty", "expression_pretty");
    lab_eval_display_pair(fields, "expression", "expression");
    lab_eval_display_pair(fields, "function", "function");
    lab_eval_alias(fields, "full_TeX", "tex");
    lab_eval_alias(fields, "value_TeX", "value_tex");
    bool scalar = !*lab_eval_text(fields, "rows") && !*lab_eval_text(fields, "cols");
    lab_eval_put(fields, "scalar", json_new_bool(scalar));
    string_t *summary =
        scalar ? string_sprintf("%s · %s", lab_eval_text(fields, "kind"), lab_eval_text(fields, "operation"))
               : string_sprintf("%s · %sx%s · %s", lab_eval_text(fields, "kind"), lab_eval_text(fields, "rows"),
                                lab_eval_text(fields, "cols"), lab_eval_text(fields, "operation"));
    if (summary)
        lab_eval_set(fields, "summary", string_c_str(summary));
    string_free(summary);
    if (lab_eval_unset_value(lab_eval_text(fields, "value")))
        lab_eval_set(fields, "value", "");
    lab_eval_render_field(fields, "tex", "svg", "render_error");
    lab_eval_render_field(fields, "value_TeX", "value_svg", "value_render_error");
}

static void lab_eval_equation_fields(json_t *fields)
{
    lab_eval_alias(fields, "equation_TeX", "tex");
    const char *const renderings[] = {"derivation_TeX", "solutions_TeX", "tex"};
    const char *const equations[] = {"unbound", "equation"};
    lab_eval_display_pair(fields, "TeX", lab_eval_first_field(fields, renderings, 3u));
    lab_eval_display_pair(fields, "equation", lab_eval_first_field(fields, equations, 2u));
    lab_eval_alias(fields, "tex", "display_TeX");
    json_t *solutions = lab_eval_line_array(fields, "solutions");
    string_t *count = string_sprintf("%zu", json_array_size(solutions));
    lab_eval_put(fields, "solution_count", json_new_number(count));
    string_free(count);
    json_free(solutions);
    lab_eval_put(fields, "numeric_solutions", lab_eval_line_array(fields, "numeric"));
    lab_eval_render_field(fields, "display_TeX", "svg", "render_error");
}

static void lab_eval_diffequation_fields(json_t *fields)
{
    const char *const renderings[] = {"display_TeX", "solutions_TeX", "problem_TeX"};
    lab_eval_alias(fields, "display_TeX", lab_eval_first_field(fields, renderings, 3u));
    if (!*lab_eval_text(fields, "display_wrapped_TeX"))
        lab_eval_alias(fields, "display_wrapped_TeX", "display_TeX");
    lab_eval_alias(fields, "steps_left_TeX", "steps_TeX");
    lab_eval_alias(fields, "steps_wrapped_TeX", "steps_TeX");
    lab_eval_render_field(fields, "display_TeX", "svg", "render_error");
    if (strcmp(lab_eval_text(fields, "display_TeX"), lab_eval_text(fields, "display_wrapped_TeX")))
        lab_eval_render_field(fields, "display_wrapped_TeX", "wrapped_svg", "wrapped_render_error");
}

static void lab_eval_integrator_fields(json_t *fields)
{
    const char *keys[] = {"bound_var", "bound_lower", "bound_upper"};
    string_t **parts[3] = {0};
    size_t counts[3] = {0};
    for (size_t i = 0u; i < 3u; ++i) {
        const string_t *text = json_string_value(lab_eval_get(fields, keys[i]));
        if (text)
            parts[i] = lab_eval_split_lines(text, &counts[i]);
    }
    json_t *bounds = json_new_array();
    for (size_t i = 0u; i < counts[0]; ++i) {
        if (!string_byte_length(parts[0][i]))
            continue;
        json_t *bound = json_new_object();
        lab_eval_set(bound, "kind", "bound");
        lab_eval_set(bound, "name", string_c_str(parts[0][i]));
        lab_eval_set(bound, "lo", i < counts[1] ? string_c_str(parts[1][i]) : "");
        lab_eval_set(bound, "hi", i < counts[2] ? string_c_str(parts[2][i]) : "");
        json_array_append(bounds, bound);
        json_free(bound);
    }
    lab_eval_put(fields, "bounds", bounds);
    for (size_t i = 0u; i < 3u; ++i) {
        lab_eval_set(fields, keys[i], counts[i] ? string_c_str(parts[i][0]) : "");
        string_split_free(parts[i], counts[i]);
    }
    if (!*lab_eval_text(fields, "binding_expression"))
        lab_eval_alias(fields, "binding_expression", "input");
    if (!*lab_eval_text(fields, "work_units"))
        lab_eval_alias(fields, "work_units", "intervals");
    if (!*lab_eval_text(fields, "work_cap"))
        lab_eval_alias(fields, "work_cap", "max_intervals");
    lab_eval_render_field(fields, "tex", "svg", "render_error");
}

typedef struct {
    const char *mode;
    void (*adapt)(json_t *fields);
} adapter_t;

static int lab_eval_compare_adapter(const void *key, const void *entry)
{
    return strcmp(key, ((const adapter_t *)entry)->mode);
}

/* Supply UI aliases while preserving all native mathematical output fields. */
void lab_eval_adapt(json_t *fields, const char *mode)
{
    static const adapter_t adapters[] = {
        {"diffequation", lab_eval_diffequation_fields}, {"equation", lab_eval_equation_fields},
        {"expression", lab_eval_expression_fields},     {"goal_seek", lab_eval_expression_fields},
        {"integrator", lab_eval_integrator_fields},     {"matrix", lab_eval_matrix_fields},
    };
    if (!fields)
        return;
    lab_eval_set(fields, "mode", mode);
    lab_eval_binding_array(fields, "bindings", "binding_values");
    const adapter_t *adapter =
        bsearch(mode, adapters, sizeof(adapters) / sizeof(*adapters), sizeof(*adapters), lab_eval_compare_adapter);
    if (adapter)
        adapter->adapt(fields);
}
