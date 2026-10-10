/**
 * @file lab_evaluate_presentation.c
 * @brief Native result layout metadata and the presentation route facade.
 *
 * Decodes bounded presentation text with string_t cursors, preserving exact
 * authored substrings. This adapter never evaluates or simplifies an expression.
 * Delegates authored-envelope actions to lab_evaluate_editor.c and supplies matrix
 * rows, calculus bodies, numerical literal classification, equation solution
 * text, integrator detail/value text and solver TeX. The
 * server owns transport and deadlines; no browser mathematical parsing is needed.
 */
#include <ctype.h>
#include <string.h>

#include "number.h"
#include "ustring.h"
#include "lab_presentation.h"
#include "lab_evaluate_internal.h"
#include "lab_presentation_internal.h"
#include "lab_syntax_internal.h"

static string_t *lab_pres_solver_TeX(const string_t *source)
{
    static const char *const escapes[128] = {
        ['#']  = "\\#",
        ['$']  = "\\$",
        ['%']  = "\\%",
        ['&']  = "\\&",
        ['\\'] = "\\textbackslash{}",
        ['_']  = "\\_",
        ['{']  = "\\{",
        ['}']  = "\\}",
        ['^']  = "\\textasciicircum{}",
        ['~']  = "\\textasciitilde{}",
    };
    size_t count = 0u;
    string_t *newline = string_new_with("\n");
    string_t **lines = newline ? string_split_string(source, newline, &count) : NULL;
    string_free(newline);
    string_t *out = lines ? string_new_with("\\begin{aligned}[t]") : NULL;
    bool ok = out != NULL;
    for (size_t i = 0u; ok && i < count; ++i) {
        string_trim(lines[i]);
        ok = (!i || !string_append_cstr(out, "\\\\")) && !string_append_cstr(out, "&\\text{");
        if (ok && !string_byte_length(lines[i]))
            ok = !string_append_cstr(out, "\\phantom{X}");
        string_cursor_t *cursor = string_cursor_new(lines[i]);
        ok = ok && cursor;
        while (ok && !string_cursor_done(cursor)) {
            size_t start = string_cursor_position(cursor);
            unsigned char byte = 0;
            bool escaped = string_cursor_peek_ascii(cursor, &byte) && escapes[byte];
            string_cursor_next(cursor);
            string_t *rune = escaped ? NULL : string_cursor_slice_between(start, string_cursor_position(cursor), cursor);
            ok = escaped ? !string_append_cstr(out, escapes[byte]) : rune && !string_append_string(out, rune);
            string_free(rune);
        }
        string_cursor_free(cursor);
        ok = ok && !string_append_char(out, '}');
    }
    ok = ok && !string_append_cstr(out, "\\end{aligned}");
    string_split_free(lines, count);
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}

static json_t *lab_pres_matrix_rows(const string_t *source)
{
    if (!string_starts_with(source, "(") || !string_ends_with(source, ")"))
        return NULL;
    string_t *body = lab_pres_slice(source, 1u, string_byte_length(source) - 1u);
    json_t *rows = body ? lab_pres_split(body, ";") : NULL;
    json_t *result = rows ? json_new_array() : NULL;
    size_t width = 0u;
    bool ok = result != NULL;
    for (size_t i = 0u; ok && i < json_array_size(rows); ++i) {
        string_t *text = string_new_with(lab_eval_text(json_array_get(rows, i), "text"));
        json_t *cells = text ? lab_pres_split(text, ",") : NULL;
        json_t *row = cells ? json_new_array() : NULL;
        size_t count = cells ? json_array_size(cells) : 0u;
        if (i == 0u)
            width = count;
        ok = row && count == width && count > 0u;
        for (size_t j = 0u; ok && j < count; ++j) {
            string_t *cell = string_new_with(lab_eval_text(json_array_get(cells, j), "text"));
            ok = cell && string_byte_length(cell) && lab_pres_append(row, json_new_string(cell));
            string_free(cell);
        }
        if (!lab_pres_append(result, row))
            ok = false;
        json_free(cells);
        string_free(text);
    }
    json_free(rows);
    string_free(body);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

static json_t *lab_pres_matrix_terms(const string_t *source)
{
    json_t *parts = lab_pres_split(source, "+");
    json_t *result = parts ? json_new_array() : NULL;
    bool ok = result != NULL;
    for (size_t i = 0u; ok && i < json_array_size(parts); ++i) {
        string_t *text = string_new_with(lab_eval_text(json_array_get(parts, i), "text"));
        json_t *rows = text ? lab_pres_matrix_rows(text) : NULL;
        string_t *factor = string_new();
        /* Native matrix products use a middle dot; ASCII dots may be decimal points. */
        if (!rows && text) {
            json_t *products = lab_pres_split(text, "*.");
            if (products && json_array_size(products) == 2u) {
                string_t *matrix = string_new_with(lab_eval_text(json_array_get(products, 1u), "text"));
                rows = matrix ? lab_pres_matrix_rows(matrix) : NULL;
                string_free(matrix);
                string_free(factor);
                factor = string_new_with(lab_eval_text(json_array_get(products, 0u), "text"));
            }
            json_free(products);
        }
        json_t *term = json_new_object();
        ok = rows && factor && term && lab_eval_set(term, "factor", string_c_str(factor));
        if (ok)
            ok = lab_eval_put(term, "rows", rows);
        else
            json_free(rows);
        if (!lab_pres_append(result, term))
            ok = false;
        string_free(factor);
        string_free(text);
    }
    json_free(parts);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

static bool lab_pres_numeric_literal(const string_t *text)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool ok = cursor != NULL, digit_or_i = false;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        if (string_cursor_peek_ascii(cursor, &byte)) {
            ok = isdigit(byte) || isspace(byte) || strchr("+-./eEiI", byte);
            digit_or_i = digit_or_i || isdigit(byte) || byte == 'i' || byte == 'I';
        } else {
            uint32_t rune = rune_value(string_cursor_peek(cursor));
            ok = (rune >= 0x2080 && rune <= 0x2089) || (rune >= 0x2070 && rune <= 0x2079) ||
                 (rune >= 0x2150 && rune <= 0x215e) || (rune >= 0xbc && rune <= 0xbe) ||
                 rune == 0xb2 || rune == 0xb3 || rune == 0xb9 || rune == 0x2044;
            digit_or_i = digit_or_i || (ok && rune != 0x2044);
        }
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (!ok || !digit_or_i)
        return false;
    number_t number = num_create_from_text(text);
    ok = num_is_finite(number);
    num_destroy(&number);
    return ok;
}

static bool lab_pres_expansions(json_t *metadata, const json_t *fields)
{
    static const char *const keys[][3] = {
        {"display_expression", "full_display_expression", "expression"},
        {"display_function", "full_display_function", "function"},
        {"display_TeX", "full_display_TeX", "tex"},
        {"display_expression_pretty", "expression_pretty", "expression"},
        {"display_value", "full_value", "value"}
    };
    json_t *entries = json_new_array();
    bool ok = entries != NULL;
    for (size_t i = 0; ok && i < sizeof(keys) / sizeof(*keys); ++i) {
        const char *display = lab_eval_text(fields, keys[i][0]);
        const char *full = lab_eval_text(fields, keys[i][1]);
        if (!*full) full = lab_eval_text(fields, keys[i][2]);
        if (!*display) display = full;
        string_t *text = string_new_with(display);
        bool abbreviated = text && (string_find(text, "...") >= 0 || string_find(text, "…") >= 0 ||
                           string_find(text, "\\ldots") >= 0 || string_find(text, "\\dots") >= 0);
        json_t *entry = json_new_object();
        ok = text && entry && lab_eval_set(entry, "display", display) && lab_eval_set(entry, "full", full) &&
             lab_eval_put(entry, "can_expand", json_new_bool(abbreviated && *full && strcmp(display, full)));
        if (!lab_pres_append(entries, entry))
            ok = false;
        string_free(text);
    }
    if (!lab_eval_put(metadata, "expansions", entries))
        ok = false;
    return ok && lab_eval_put(metadata, "responsive_fit",
                              json_new_bool(*lab_eval_text(fields, "display_wrapped_svg") != '\0'));
}

/* Work counters accept both native text and JSON numbers; numeric zero is an absent counter. */
static string_t *lab_pres_work_counter(const json_t *fields, const char *key, const char *fallback)
{
    const json_t *value = lab_eval_get(fields, key);
    const string_t *text = json_string_value(value);
    if (text && string_byte_length(text)) return string_clone(text);
    number_t number;
    if (json_number_value(value, &number)) {
        bool present = num_to_double(number) != 0;
        string_t *formatted = present ? num_to_string(number) : NULL;
        num_destroy(&number);
        if (present) return formatted;
    }
    return fallback ? lab_pres_work_counter(fields, fallback, NULL) : string_new();
}

static bool lab_pres_integrator_text(json_t *metadata, const json_t *fields)
{
    const char *status = lab_eval_text(fields, "status");
    string_t *folded = string_new_with(status), *detail = string_new(), *domain = string_new(), *value = string_new();
    json_t *integrator = json_new_object();
    bool ok = folded && detail && domain && value && integrator;
    if (folded) string_to_lower(folded);
    bool antiderivative_status = folded && string_find(folded, "antiderivative") >= 0;
    bool symbolic_status = antiderivative_status || (folded && (string_find(folded, "symbolic") >= 0 ||
                           string_find(folded, "closed-form") >= 0 || string_find(folded, "fast path") >= 0));
    const char *antiderivative = lab_eval_text(fields, "antiderivative"), *symbolic = lab_eval_text(fields, "symbolic");
    if (ok && *antiderivative) ok = string_append_format(detail, "Antiderivative:\n%s", antiderivative) >= 0;
    if (ok && *symbolic && !antiderivative_status) {
        string_t *line = string_sprintf("Definite result:\n%s", symbolic);
        ok = line && lab_pres_join(detail, string_c_str(line), "\n\n");
        string_free(line);
    }
    if (ok) ok = lab_pres_join(domain, lab_eval_text(fields, "bound"), "\n");
    if (ok && *status) {
        string_t *line = string_sprintf("status: %s", status);
        ok = line && lab_pres_join(domain, string_c_str(line), "\n");
        string_free(line);
    }
    string_t *units_text = lab_pres_work_counter(fields, "work_units", "intervals");
    string_t *cap_text = lab_pres_work_counter(fields, "work_cap", "max_intervals");
    ok = ok && units_text && cap_text;
    const char *units = units_text ? string_c_str(units_text) : "";
    const char *cap = cap_text ? string_c_str(cap_text) : "";
    if (ok && !symbolic_status && *units) {
        string_t *line = *cap ? string_sprintf("work used: %s / %s%s", units, cap,
                                              strcmp(units, cap) ? " (precision reached)" : "") :
                               string_sprintf("work used: %s", units);
        ok = line && lab_pres_join(domain, string_c_str(line), "\n");
        string_free(line);
    }
    if (ok && string_byte_length(domain)) ok = lab_pres_join(detail, string_c_str(domain), "\n\n");
    if (ok) ok = lab_pres_join(value, lab_eval_text(fields, "value"), "\n");
    const char *error = lab_eval_text(fields, "error");
    if (ok && *error) {
        string_t *line = string_sprintf("error ≈ %s", error);
        ok = line && lab_pres_join(value, string_c_str(line), "\n");
        string_free(line);
    }
    if (ok && !string_byte_length(value)) ok = string_append_cstr(value, status) == 0;
    ok = ok && lab_eval_set(integrator, "detail_text", string_c_str(detail)) &&
         lab_eval_set(integrator, "value_text", string_c_str(value));
    string_free(folded);
    string_free(detail);
    string_free(domain);
    string_free(value);
    string_free(units_text);
    string_free(cap_text);
    if (!ok) { json_free(integrator); return false; }
    return lab_eval_put(metadata, "integrator", integrator);
}

static bool lab_pres_solution_line(string_t *text, size_t *count, const string_t *line)
{
    if (*count && string_append_char(text, '\n')) return false;
    if (string_append_string(text, line)) return false;
    ++*count;
    return true;
}

/* Assemble the Value card from native literal flags, not a second mathematical parser. */
static string_t *lab_pres_equation_solution_text(const json_t *fields, const json_t *solutions)
{
    if (!strcmp(lab_eval_text(fields, "status"), "no solutions")) return string_new_with("No solutions");
    string_t *text = string_new(), *blank = string_new(), *newline = string_new_with("\n");
    size_t count = 0u, line_count = 0u;
    bool has_blank = false, ok = text && blank && newline;
    const string_t *note = json_string_value(lab_eval_get(fields, "interpretation_note"));
    if (ok && note && string_byte_length(note)) {
        ok = lab_pres_solution_line(text, &count, note) && lab_pres_solution_line(text, &count, blank);
        has_blank = true;
    }
    const string_t *display = json_string_value(lab_eval_get(fields, "display_solutions"));
    if (!display || !string_byte_length(display)) display = json_string_value(lab_eval_get(fields, "solutions"));
    string_t **lines = ok && display ? string_split_string(display, newline, &line_count) : NULL;
    if (ok && display && !lines) ok = false;
    for (size_t i = 0u; ok && i < line_count; ++i) {
        string_trim(lines[i]);
        if (string_byte_length(lines[i])) ok = lab_pres_solution_line(text, &count, lines[i]);
    }
    string_split_free(lines, line_count);
    const json_t *numeric = lab_eval_get(fields, "numeric_solutions");
    size_t index = 0u;
    for (size_t i = 0u; ok && json_type(numeric) == JSON_ARRAY && i < json_array_size(numeric); ++i) {
        const string_t *value = json_string_value(json_array_get(numeric, i));
        string_t *line = value ? string_clone(value) : NULL;
        if (!line) { ok = false; break; }
        string_trim(line);
        if (string_byte_length(line)) {
            bool literal = false;
            json_bool_value(lab_eval_get(json_array_get(solutions, index++), "numeric"), &literal);
            if (!literal) {
                if (count && !has_blank) {
                    ok = lab_pres_solution_line(text, &count, blank);
                    has_blank = true;
                }
                ok = ok && lab_pres_solution_line(text, &count, line);
            }
        }
        string_free(line);
    }
    const string_t *status = json_string_value(lab_eval_get(fields, "status"));
    if (ok && !count && status && string_byte_length(status)) ok = lab_pres_solution_line(text, &count, status);
    static const char *const notes[] = {"search_note", "family_note"};
    for (size_t i = 0u; ok && i < 2u; ++i) {
        note = json_string_value(lab_eval_get(fields, notes[i]));
        if (!note || !string_byte_length(note)) continue;
        if (count) ok = lab_pres_solution_line(text, &count, blank);
        ok = ok && lab_pres_solution_line(text, &count, note);
    }
    string_free(blank);
    string_free(newline);
    if (!ok) { string_free(text); return NULL; }
    return text;
}

/* Enrich completed native responses without changing their mathematical representations. */
bool lab_presentation_adapt(json_t *fields)
{
    if (!fields || json_type(fields) != JSON_OBJECT)
        return false;
    json_t *metadata = json_new_object(), *calculus = json_new_array(), *solutions = json_new_array();
    bool ok = metadata && calculus && solutions &&
              lab_pres_calculus_cards(metadata, fields, calculus);
    string_t *solution_text = string_new_with(lab_eval_text(fields, "solutions"));
    size_t count = 0u;
    string_t **lines = solution_text ? string_split(solution_text, "\n", &count) : NULL;
    for (size_t i = 0u; ok && lines && i < count; ++i) {
        string_trim(lines[i]);
        if (!string_byte_length(lines[i]))
            continue;
        string_offset_t position = string_find(lines[i], "=");
        size_t separator_size = 1u;
        if (position < 0) {
            position = string_find(lines[i], "≈");
            separator_size = 3u;
        }
        string_t *rhs = position < 0 ? NULL :
            lab_pres_slice(lines[i], (size_t)position + separator_size, string_byte_length(lines[i]));
        bool numeric = rhs && lab_pres_numeric_literal(rhs);
        json_t *entry = json_new_object();
        ok = entry && lab_eval_set(entry, "line", string_c_str(lines[i])) &&
             lab_eval_put(entry, "numeric", json_new_bool(numeric));
        if (!lab_pres_append(solutions, entry))
            ok = false;
        string_free(rhs);
    }
    string_split_free(lines, count);
    string_free(solution_text);
    string_t *equation_text = ok ? lab_pres_equation_solution_text(fields, solutions) : NULL;
    ok = ok && equation_text && lab_eval_set(metadata, "equation_solution_text", string_c_str(equation_text));
    string_free(equation_text);
    ok = ok && lab_pres_integrator_text(metadata, fields);
    string_t *solver = string_new();
    const char *symmetry = lab_eval_text(fields, "symmetry"), *steps = lab_eval_text(fields, "steps");
    if (solver && (*symmetry || *steps)) {
        if (*symmetry)
            ok = ok && string_append_format(solver, "Symmetry: %s", symmetry) >= 0;
        if (*steps)
            ok = ok && lab_pres_join(solver, steps, "\n\n");
    } else if (solver) {
        const char *labels[] = {"solver", "status"};
        for (size_t i = 0u; ok && i < 2u; ++i) {
            const char *value = lab_eval_text(fields, labels[i]);
            string_t *line = *value ? string_sprintf("%s: %s", labels[i], value) : NULL;
            if (*value)
                ok = line && lab_pres_join(solver, string_c_str(line), "\n");
            string_free(line);
        }
        const char *diagnostic = lab_eval_text(fields, "diagnostic");
        if (*diagnostic)
            ok = ok && lab_pres_join(solver, diagnostic, "\n");
    }
    string_t *TeX = solver ? lab_pres_solver_TeX(solver) : NULL;
    ok = ok && solver && TeX && lab_eval_set(metadata, "solver_text", string_c_str(solver)) &&
         lab_eval_set(metadata, "solver_TeX", string_c_str(TeX));
    if (!lab_eval_put(metadata, "calculus_lines", calculus))
        ok = false;
    if (!lab_eval_put(metadata, "solution_lines", solutions))
        ok = false;
    json_t *editors = json_new_array();
    static const char *const editor_fields[] = {
        "input", "expression", "binding_expression", "unbound_expression", "result", "display_expression",
        "full_display_expression"
    };
    for (size_t i = 0; editors && i < sizeof(editor_fields) / sizeof(*editor_fields); ++i) {
        const string_t *source = json_string_value(lab_eval_get(fields, editor_fields[i]));
        if (!source || string_byte_length(source) > 65536u)
            continue;
        json_t *editor = lab_pres_editor_metadata(source);
        /* Diagnostic/non-expression fields need not be valid editor envelopes. */
        if (editor && !lab_pres_append(editors, editor))
            ok = false;
    }
    if (!lab_eval_put(metadata, "editors", editors))
        ok = false;
    json_t *syntax = json_new_array();
    static const char *const function_fields[] = {
        "display_function", "full_display_function", "function",
        "display_derivative_function", "full_display_derivative_function", "derivative_function",
        "display_integral_function", "full_display_integral_function", "integral_function"
    };
    for (size_t i = 0u; syntax && i < sizeof(function_fields) / sizeof(*function_fields); ++i) {
        const string_t *source = json_string_value(lab_eval_get(fields, function_fields[i]));
        if (!source || !string_byte_length(source)) continue;
        bool duplicate = false;
        /* Nine fixed display fields; at most 1024 spans each keeps wire node use bounded. */
        for (size_t j = 0u; j < i; ++j) {
            const string_t *earlier = json_string_value(lab_eval_get(fields, function_fields[j]));
            if (earlier && !string_compare(source, earlier)) duplicate = true;
        }
        if (!duplicate && !lab_pres_append(syntax, lab_pres_function_syntax(source))) ok = false;
    }
    if (!lab_eval_put(metadata, "function_syntax", syntax)) ok = false;
    json_t *headings = json_new_array();
    static const char *const heading_fields[] = {
        "display_expression", "full_display_expression", "expression", "result", "display_result",
        "display_expression_pretty", "expression_pretty", "display_value", "full_value", "value",
        "display_function", "full_display_function", "function"
    };
    for (size_t i = 0u; headings && i < sizeof(heading_fields) / sizeof(*heading_fields); ++i) {
        const string_t *source = json_string_value(lab_eval_get(fields, heading_fields[i]));
        if (!source || !string_byte_length(source)) continue;
        bool duplicate = false;
        /* Thirteen fixed fields, deduplicated to bound repeated wire metadata. */
        for (size_t j = 0u; j < i; ++j) {
            const string_t *earlier = json_string_value(lab_eval_get(fields, heading_fields[j]));
            if (earlier && !string_compare(source, earlier)) duplicate = true;
        }
        if (!duplicate && !lab_pres_append(headings, lab_pres_matrix_headings(source))) ok = false;
    }
    if (!lab_eval_put(metadata, "matrix_headings", headings)) ok = false;
    ok = ok && lab_pres_expansions(metadata, fields);
    if (ok)
        ok = lab_eval_put(fields, "presentation", metadata);
    else
        json_free(metadata);
    string_free(TeX);
    string_free(solver);
    return ok;
}

static bool lab_pres_request_text(const json_t *value, unsigned depth)
{
    if (depth > 16u)
        return false;
    const string_t *text = json_string_value(value);
    if (text)
        return strlen(string_c_str(text)) == string_byte_length(text);
    if (json_type(value) == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(value); ++i)
            if (!lab_pres_request_text(json_array_get(value, i), depth + 1u))
                return false;
    } else if (json_type(value) == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(value); ++i) {
            const string_t *key = json_object_key_at(value, i);
            if (strlen(string_c_str(key)) != string_byte_length(key) ||
                !lab_pres_request_text(json_object_value_at(value, i), depth + 1u))
                return false;
        }
    }
    return true;
}

/* Handle small presentation operations through the server's existing typed transport. */
json_t *lab_presentation_request(const json_t *payload, unsigned *status)
{
    if (status)
        *status = 400u;
    json_t *result = json_new_object();
    if (!result)
        goto allocation_failure;
    const char *action = lab_eval_text(payload, "action");
    const char *key = !strcmp(action, "unset_constants") ? "expression" : "text";
    const string_t *borrowed = json_string_value(lab_eval_get(payload, key));
    if (!payload || json_type(payload) != JSON_OBJECT || !borrowed || string_byte_length(borrowed) > 65536u ||
        !lab_pres_request_text(payload, 0u)) {
        lab_eval_put(result, "ok", json_new_bool(false));
        lab_eval_set(result, "error", "Presentation requests require NUL-free text of at most 64 KiB.");
        return result;
    }
    string_t *text = string_new_with(string_c_str(borrowed));
    if (!text)
        goto allocation_failure;
    bool ok = true, valid = true;
    if (!strcmp(action, "editor")) {
        json_t *editor = lab_pres_editor_action(payload, text);
        valid = editor != NULL;
        if (editor)
            ok = lab_eval_put(result, "editor", editor);
    } else if (!strcmp(action, "matrix")) {
        string_trim(text);
        json_t *terms = lab_pres_matrix_terms(text);
        string_t *html = lab_pres_matrix_markup(terms);
        ok = html && lab_eval_set(result, "html", string_c_str(html));
        string_free(html);
        if (!lab_eval_put(result, "terms", terms ? terms : json_new_array())) ok = false;
    } else if (!strcmp(action, "solver_text")) {
        string_t *TeX = lab_pres_solver_TeX(text);
        ok = TeX && lab_eval_set(result, "tex", string_c_str(TeX));
        string_free(TeX);
    } else if (!strcmp(action, "unset_constants")) {
        const json_t *names = lab_eval_get(payload, "names");
        valid = names && json_type(names) == JSON_ARRAY && json_array_size(names) <= 256u;
        for (size_t i = 0u; valid && ok && i < json_array_size(names); ++i) {
            const string_t *name = json_string_value(json_array_get(names, i));
            if (!name || !string_byte_length(name) || string_byte_length(name) > 256u ||
                strlen(string_c_str(name)) != string_byte_length(name)) {
                valid = false;
                break;
            }
            string_t *next = lab_pres_unset_one(text, name);
            if (!next) {
                valid = false;
                break;
            }
            string_free(text);
            text = next;
        }
        if (valid)
            ok = lab_eval_set(result, "expression", string_c_str(text));
    } else {
        valid = false;
    }
    string_free(text);
    if (!ok)
        goto allocation_failure;
    if (!lab_eval_put(result, "ok", json_new_bool(valid)))
        goto allocation_failure;
    if (!valid) {
        if (!lab_eval_set(result, "error", "Unsupported presentation action or malformed binding envelope."))
            goto allocation_failure;
        return result;
    }
    if (status)
        *status = 200u;
    return result;

allocation_failure:
    json_free(result);
    if (status)
        *status = 500u;
    return NULL;
}
