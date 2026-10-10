/**
 * @file lab_evaluate_editor.c
 * @brief Native authored binding analysis, editor actions and numeric display compaction.
 *
 * Parses bounded envelopes with string_t cursors, retains exact symbolic values
 * and conditions, and supplies editor metadata without evaluating the algebra.
 * Explicit batch edits, matrix calculus input construction and goal-seek preparation use the public
 * lab_presentation.h facade. Private lexical helpers also support matrix layout.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "lab_evaluate_internal.h"
#include "lab_presentation_internal.h"
#include "ustring.h"

/* Consume a temporary JSON node after appending its copy. */
bool lab_pres_append(json_t *array, json_t *value)
{
    bool ok = array && value && json_array_append(array, value);
    json_free(value);
    return ok;
}

/* Copy and trim a bounded encoded-byte slice. */
string_t *lab_pres_slice(const string_t *source, size_t start, size_t end)
{
    string_t *text = string_substr(source, start, end - start);
    if (text)
        string_trim(text);
    return text;
}

/* A single bounded syntax scan; square-bracket names and quoted text stay opaque. */
json_t *lab_pres_split(const string_t *source, const char *separators)
{
    json_t *parts = json_new_array();
    string_cursor_t *cursor = string_cursor_new(source);
    unsigned char stack[128];
    size_t depth = 0u, start = 0u;
    unsigned char quote = 0, previous = 0, before_previous = 0;
    bool escaped = false, ok = parts && cursor;
    char separator[2] = {0};
    while (ok && !string_cursor_done(cursor)) {
        size_t position = string_cursor_position(cursor);
        unsigned char byte = 0;
        bool ascii = string_cursor_peek_ascii(cursor, &byte);
        bool middle_dot = !ascii && rune_value(string_cursor_peek(cursor)) == 0xb7 && strchr(separators, '*');
        size_t separator_bytes = middle_dot ? 2u : 1u;
        bool split = false;
        if (quote) {
            if (escaped)
                escaped = false;
            else if (byte == '\\')
                escaped = true;
            else if (byte == quote)
                quote = 0;
        } else if (ascii && byte == '"') {
            quote = byte;
        } else if (ascii && (byte == '(' || byte == '[' || byte == '{')) {
            if (depth == sizeof(stack)) {
                ok = false;
                break;
            }
            stack[depth++] = byte;
        } else if (ascii && (byte == ')' || byte == ']' || byte == '}')) {
            unsigned char wanted = byte == ')' ? '(' : byte == ']' ? '[' : '{';
            if (!depth || stack[--depth] != wanted) {
                ok = false;
                break;
            }
        } else if (!depth && (middle_dot || (ascii && strchr(separators, byte)))) {
            split = true;
            if (middle_dot)
                byte = '*';
            if (byte == '.') {
                unsigned char next = 0;
                string_view_peek_ascii(string_view_all(source), position + 1u, &next);
                split = !isdigit(next);
            }
            if (byte == '+' || byte == '-') {
                bool exponent =
                    (previous == 'e' || previous == 'E') && (isdigit(before_previous) || before_previous == '.');
                bool unary = !previous || strchr("+-*/^=,;", previous);
                split = !exponent && !unary;
            }
        }
        if (split) {
            string_t *text = lab_pres_slice(source, start, position);
            json_t *part = json_new_object();
            ok = text && part && lab_eval_set(part, "text", string_c_str(text)) &&
                 lab_eval_set(part, "separator", separator);
            string_free(text);
            if (!lab_pres_append(parts, part))
                ok = false;
            separator[0] = (char)byte;
            start = position + separator_bytes;
        }
        if (ascii && !isspace(byte)) {
            before_previous = previous;
            previous = byte;
        } else if (!ascii) {
            before_previous = previous;
            previous = 0xff;
        }
        string_cursor_next(cursor);
    }
    if (quote || depth)
        ok = false;
    if (ok) {
        string_t *text = lab_pres_slice(source, start, string_byte_length(source));
        json_t *part = json_new_object();
        ok = text && part && lab_eval_set(part, "text", string_c_str(text)) &&
             lab_eval_set(part, "separator", separator);
        string_free(text);
        if (!lab_pres_append(parts, part))
            ok = false;
    }
    string_cursor_free(cursor);
    if (!ok) {
        json_free(parts);
        parts = NULL;
    }
    return parts;
}

static bool lab_pres_integration_name(const string_t *name)
{
    string_cursor_t *cursor = string_cursor_new(name);
    bool ok = cursor && string_cursor_consume(cursor, "C");
    bool underscore = ok && string_cursor_consume(cursor, "_");
    bool digit = false;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        if (underscore) {
            ok = string_cursor_peek_ascii(cursor, &byte) && byte >= '0' && byte <= '9';
            if (ok)
                string_cursor_next(cursor);
        } else {
            uint32_t rune = rune_value(string_cursor_peek(cursor));
            ok = rune >= 0x2080 && rune <= 0x2089;
            if (ok)
                string_cursor_next(cursor);
        }
        digit = digit || ok;
    }
    bool result = ok && (!underscore || digit);
    string_cursor_free(cursor);
    return result;
}

static bool lab_pres_identifier(const string_t *name)
{
    if (!name || !string_byte_length(name) || string_byte_length(name) > 256u ||
        strlen(string_c_str(name)) != string_byte_length(name))
        return false;
    /* Reject operators before entering the parser: validating a name must not execute calculus. */
    bool bracketed = (string_starts_with(name, "[") || string_starts_with(name, "$[")) && string_ends_with(name, "]");
    string_cursor_t *cursor = string_cursor_new(name);
    bool ok = cursor != NULL;
    while (ok && !bracketed && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        if (string_cursor_peek_ascii(cursor, &byte))
            ok = isalnum(byte) || byte == '_';
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (!ok)
        return false;
    string_t *probe = string_sprintf("{ %S | %S = ? }", name, name);
    expr_bindings_t *bindings = NULL;
    expr_t *expression = probe ? expr_from_text(probe, &bindings) : NULL;
    ok = expression && expr_is_variable(expression) && expr_bindings_count(bindings) == 1u &&
         expr_bindings_get_text(bindings, name) != NULL;
    expr_free(expression);
    expr_bindings_free(bindings);
    string_free(probe);
    return ok;
}

static bool lab_pres_assignment(const string_t *text, string_t **name)
{
    json_t *parts = lab_pres_split(text, "=");
    bool ok = parts && json_array_size(parts) == 2u;
    if (ok) {
        const char *left = lab_eval_text(json_array_get(parts, 0u), "text");
        string_t *candidate = string_new_with(left);
        ok = candidate && lab_pres_identifier(candidate);
        if (ok)
            *name = candidate;
        else
            string_free(candidate);
    }
    json_free(parts);
    return ok;
}

/* Join retained exact substrings with an explicit separator. */
bool lab_pres_join(string_t *out, const char *text, const char *separator)
{
    return (!string_byte_length(out) || !string_append_cstr(out, separator)) && !string_append_cstr(out, text);
}

static string_t *lab_pres_remove_term(const string_t *body, const string_t *name)
{
    json_t *terms = lab_pres_split(body, "+-");
    string_t *result = terms ? string_new() : NULL;
    bool ok = result != NULL;
    for (size_t i = 0u; ok && i < json_array_size(terms); ++i) {
        const json_t *term = json_array_get(terms, i);
        string_t *text = string_new_with(lab_eval_text(term, "text"));
        const char *separator = lab_eval_text(term, "separator");
        bool negative = !strcmp(separator, "-");
        if (text && i == 0u && (string_starts_with(text, "+") || string_starts_with(text, "-"))) {
            negative = string_starts_with(text, "-");
            string_t *unsigned_text = lab_pres_slice(text, 1u, string_byte_length(text));
            string_free(text);
            text = unsigned_text;
        }
        if (!text) {
            ok = false;
            break;
        }
        if (!string_view_equals_view(string_view_all(text), string_view_all(name))) {
            if (string_byte_length(result))
                ok = !string_append_cstr(result, negative ? " - " : " + ");
            else if (negative)
                ok = !string_append_char(result, '-');
            ok = ok && !string_append_string(result, text);
        }
        string_free(text);
    }
    if (ok && !string_byte_length(result))
        ok = !string_append_char(result, '0');
    json_free(terms);
    if (!ok) {
        string_free(result);
        result = NULL;
    }
    return result;
}

/* Remove only the selected integration constant's binding and bare additive terms. */
string_t *lab_pres_unset_one(const string_t *source, const string_t *name)
{
    if (!lab_pres_integration_name(name))
        return string_new_with(string_c_str(source));
    string_t *envelope = string_new_with(string_c_str(source));
    if (!envelope)
        return NULL;
    string_trim(envelope);
    if (string_starts_with(envelope, "{") && string_ends_with(envelope, "}")) {
        string_t *inner = lab_pres_slice(envelope, 1u, string_byte_length(envelope) - 1u);
        string_free(envelope);
        envelope = inner;
    }
    json_t *halves = envelope ? lab_pres_split(envelope, "|") : NULL;
    string_free(envelope);
    if (!halves || json_array_size(halves) != 2u) {
        json_free(halves);
        return NULL;
    }
    string_t *body = string_new_with(lab_eval_text(json_array_get(halves, 0u), "text"));
    string_t *binding_text = string_new_with(lab_eval_text(json_array_get(halves, 1u), "text"));
    json_t *assignments = binding_text ? lab_pres_split(binding_text, ",;") : NULL;
    string_free(binding_text);
    string_t *variables = string_new(), *constants = string_new(), *conditions = string_new();
    bool constant = false, removed = false, in_conditions = false;
    unsigned sections = 0u;
    bool ok = body && assignments && variables && constants && conditions;
    for (size_t i = 0u; ok && i < json_array_size(assignments); ++i) {
        const json_t *assignment = json_array_get(assignments, i);
        const char *separator = lab_eval_text(assignment, "separator");
        const char *raw = lab_eval_text(assignment, "text");
        if (!strcmp(separator, ";")) {
            constant = true;
            in_conditions = in_conditions || ++sections > 1u;
        }
        if (!*raw)
            continue;
        string_t *text = string_new_with(raw), *binding_name = NULL;
        if (!text) {
            ok = false;
            break;
        }
        if (!in_conditions && lab_pres_assignment(text, &binding_name)) {
            if (constant && string_view_equals_view(string_view_all(binding_name), string_view_all(name)))
                removed = true;
            else
                ok = lab_pres_join(constant ? constants : variables, raw, ", ");
        } else {
            in_conditions = true;
            ok = lab_pres_join(conditions, raw, "; ");
        }
        string_free(binding_name);
        string_free(text);
    }
    string_t *result = NULL;
    if (ok && !removed) {
        result = string_new_with(string_c_str(source));
    } else if (ok) {
        string_t *reduced = lab_pres_remove_term(body, name);
        result = reduced ? string_new_with("{ ") : NULL;
        ok = result && !string_append_string(result, reduced) && !string_append_cstr(result, " | ") &&
             !string_append_string(result, variables);
        if (ok && (string_byte_length(constants) || string_byte_length(conditions)))
            ok = !string_append_cstr(result, "; ") && !string_append_string(result, constants);
        if (ok && string_byte_length(conditions))
            ok = !string_append_cstr(result, "; ") && !string_append_string(result, conditions);
        if (ok)
            ok = !string_append_cstr(result, " }");
        if (ok && !string_byte_length(variables) && !string_byte_length(constants) && !string_byte_length(conditions)) {
            string_free(result);
            result = reduced;
            reduced = NULL;
        }
        string_free(reduced);
        if (!ok) {
            string_free(result);
            result = NULL;
        }
    }
    string_free(conditions);
    string_free(constants);
    string_free(variables);
    string_free(body);
    json_free(assignments);
    json_free(halves);
    return result;
}

static bool lab_pres_unset(const char *value)
{
    return !*value || !strcmp(value, "?") ||
           ((value[0] == 'n' || value[0] == 'N') && (value[1] == 'a' || value[1] == 'A') &&
            (value[2] == 'n' || value[2] == 'N') && !value[3]);
}

static bool lab_pres_editor_binding_valid(const json_t *binding)
{
    const string_t *name = json_string_value(lab_eval_get(binding, "name"));
    const string_t *value = json_string_value(lab_eval_get(binding, "value"));
    const char *kind = lab_eval_text(binding, "kind");
    if (!lab_pres_identifier(name) || !value || strlen(string_c_str(value)) != string_byte_length(value) ||
        string_byte_length(value) > 65536u || (strcmp(kind, "variable") && strcmp(kind, "constant")))
        return false;
    json_t *name_parts = lab_pres_split(name, ",;|=+-*/^");
    json_t *value_parts = lab_pres_split(value, ",;|=");
    bool ok = name_parts && value_parts && json_array_size(name_parts) == 1u && json_array_size(value_parts) == 1u;
    json_free(name_parts);
    json_free(value_parts);
    return ok;
}

static size_t lab_pres_decimal_end(string_view_t view, size_t position)
{
    unsigned char byte = 0;
    while (string_view_peek_ascii(view, position, &byte) && isdigit(byte))
        ++position;
    return position;
}

static string_t *lab_pres_compact(const string_t *source)
{
    string_t *out = string_new();
    string_view_t view = string_view_all(source);
    size_t length = string_view_length(view), position = 0u;
    uint32_t previous = 0u;
    unsigned bracket = 0u;
    bool quoted = false, escaped = false, ok = out != NULL;
    while (ok && position < length) {
        unsigned char byte = 0, next = 0;
        string_view_peek_ascii(view, position, &byte);
        size_t start = position, digits = position;
        if (byte == '+' || byte == '-')
            ++digits;
        bool candidate =
            !bracket && !quoted &&
            !(previous < 128u && (isalnum((unsigned char)previous) || previous == '_' || previous == '.')) &&
            previous < 128u && string_view_peek_ascii(view, digits, &next) && isdigit(next);
        size_t integer_end = candidate ? lab_pres_decimal_end(view, digits) : digits;
        size_t end = integer_end;
        bool decimal = false, exponent = false;
        if (candidate && string_view_peek_ascii(view, end, &next) && next == '.' &&
            string_view_peek_ascii(view, end + 1u, &next) && isdigit(next)) {
            decimal = true;
            end = lab_pres_decimal_end(view, end + 1u);
        }
        if (candidate && string_view_peek_ascii(view, end, &next) && (next == 'e' || next == 'E')) {
            size_t exponent_start = end + 1u;
            if (string_view_peek_ascii(view, exponent_start, &next) && (next == '+' || next == '-'))
                ++exponent_start;
            size_t exponent_end = lab_pres_decimal_end(view, exponent_start);
            if (exponent_end > exponent_start) {
                exponent = true;
                end = exponent_end;
            }
        }
        if (candidate && end - start > 20u && (decimal || integer_end - digits >= 21u)) {
            size_t significant = digits;
            while (significant < integer_end && string_view_peek_ascii(view, significant, &next) && next == '0')
                ++significant;
            if (!decimal && !exponent && integer_end - significant > 23u) {
                string_t *head = string_substr(source, significant, 1u);
                string_t *tail = string_substr(source, significant + 1u, 22u);
                ok = (digits == start || !string_append_char(out, (char)byte)) && head && tail &&
                     !string_append_string(out, head) && !string_append_char(out, '.') &&
                     !string_append_string(out, tail) &&
                     string_append_format(out, "...e+%zu", integer_end - significant - 1u) >= 0;
                string_free(head);
                string_free(tail);
            } else if (decimal || exponent) {
                string_t *head = string_substr(source, start, 16u);
                ok = head && !string_append_string(out, head) && !string_append_cstr(out, "...");
                string_free(head);
            } else {
                string_t *whole = string_substr(source, start, end - start);
                ok = whole && !string_append_string(out, whole);
                string_free(whole);
            }
            position = end;
            previous = '0';
            continue;
        }
        uint32_t rune = 0;
        size_t after = position;
        ok = string_view_peek_rune_value(view, position, &rune, &after) && after > position;
        string_t *part = ok ? string_substr(source, position, after - position) : NULL;
        ok = ok && part && !string_append_string(out, part);
        string_free(part);
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (byte == '\\')
                escaped = true;
            else if (byte == '"')
                quoted = false;
        } else if (byte == '"')
            quoted = true;
        else if (byte == '[')
            ++bracket;
        else if (byte == ']' && bracket)
            --bracket;
        position = after;
        previous = rune;
    }
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}

static int lab_pres_binding_compare(const void *left, const void *right)
{
    const json_t *a = *(const json_t *const *)left, *b = *(const json_t *const *)right;
    bool ac = !strcmp(lab_eval_text(a, "kind"), "constant"), bc = !strcmp(lab_eval_text(b, "kind"), "constant");
    if (ac != bc)
        return ac ? 1 : -1;
    const string_t *an = json_string_value(lab_eval_get(a, "name"));
    const string_t *bn = json_string_value(lab_eval_get(b, "name"));
    string_view_t av = string_view_all(an), bv = string_view_all(bn);
    size_t ai = 0u, bi = 0u, al = string_view_length(av), bl = string_view_length(bv);
    while (ai < al && bi < bl) {
        unsigned char x = 0, y = 0;
        string_view_peek_ascii(av, ai, &x);
        string_view_peek_ascii(bv, bi, &y);
        if (isdigit(x) && isdigit(y)) {
            size_t ae = ai, be = bi, az = ai, bz = bi;
            while (ae < al && string_view_peek_ascii(av, ae, &x) && isdigit(x))
                ++ae;
            while (be < bl && string_view_peek_ascii(bv, be, &y) && isdigit(y))
                ++be;
            while (az < ae && string_view_peek_ascii(av, az, &x) && x == '0')
                ++az;
            while (bz < be && string_view_peek_ascii(bv, bz, &y) && y == '0')
                ++bz;
            if (ae - az != be - bz)
                return ae - az < be - bz ? -1 : 1;
            while (az < ae && bz < be) {
                string_view_peek_ascii(av, az++, &x);
                string_view_peek_ascii(bv, bz++, &y);
                if (x != y)
                    return x < y ? -1 : 1;
            }
            ai = ae;
            bi = be;
        } else {
            uint32_t ar = 0, br = 0;
            string_view_peek_rune_value(av, ai, &ar, &ai);
            string_view_peek_rune_value(bv, bi, &br, &bi);
            if (ar < 128u)
                ar = (uint32_t)tolower((unsigned char)ar);
            if (br < 128u)
                br = (uint32_t)tolower((unsigned char)br);
            if (ar != br)
                return ar < br ? -1 : 1;
        }
    }
    if (ai < al || bi < bl)
        return ai < al ? 1 : -1;
    return string_compare(an, bn);
}

static json_t *lab_pres_editor_parse(const string_t *source)
{
    json_t *model = json_new_object(), *bindings = json_new_array(), *conditions = json_new_array();
    string_t *body = string_new_with(string_c_str(source));
    bool ok = model && bindings && conditions && body, wrapped = false;
    for (unsigned level = 0u; ok && level < 128u; ++level) {
        string_trim(body);
        string_t *inner = string_starts_with(body, "{") && string_ends_with(body, "}")
                              ? lab_pres_slice(body, 1u, string_byte_length(body) - 1u)
                              : string_new_with(string_c_str(body));
        json_t *halves = inner ? lab_pres_split(inner, "|") : NULL;
        string_free(inner);
        if (!halves) {
            ok = false;
            break;
        }
        if (json_array_size(halves) == 1u) {
            json_free(halves);
            break;
        }
        ok = json_array_size(halves) == 2u;
        wrapped = true;
        string_t *tail = ok ? string_new_with(lab_eval_text(json_array_get(halves, 1u), "text")) : NULL;
        json_t *parts = tail ? lab_pres_split(tail, ",;") : NULL;
        string_free(tail);
        ok = ok && parts;
        bool constant = false, in_conditions = false;
        unsigned sections = 0u;
        for (size_t i = 0u; ok && i < json_array_size(parts); ++i) {
            const json_t *part = json_array_get(parts, i);
            if (!strcmp(lab_eval_text(part, "separator"), ";")) {
                constant = true;
                in_conditions = in_conditions || ++sections > 1u;
            }
            const char *raw = lab_eval_text(part, "text");
            if (!*raw)
                continue;
            string_t *text = string_new_with(raw), *name = NULL;
            bool assignment = text && !in_conditions && lab_pres_assignment(text, &name);
            if (assignment) {
                json_t *sides = lab_pres_split(text, "=");
                const char *value = lab_eval_text(json_array_get(sides, 1u), "text");
                json_t *binding = json_new_object();
                ok = binding && lab_eval_set(binding, "name", string_c_str(name)) &&
                     lab_eval_set(binding, "value", lab_pres_unset(value) ? "?" : value) &&
                     lab_eval_set(binding, "display", lab_pres_unset(value) ? "?" : value) &&
                     lab_eval_set(binding, "kind", constant ? "constant" : "variable") &&
                     lab_eval_put(binding, "unset", json_new_bool(lab_pres_unset(value)));
                if (!lab_pres_append(bindings, binding))
                    ok = false;
                json_free(sides);
            } else {
                in_conditions = true;
                ok = text && lab_pres_append(conditions, json_new_string(text));
            }
            string_free(name);
            string_free(text);
        }
        string_t *next = ok ? string_new_with(lab_eval_text(json_array_get(halves, 0u), "text")) : NULL;
        json_free(parts);
        json_free(halves);
        string_free(body);
        body = next;
        ok = ok && body;
    }
    ok =
        ok && lab_eval_set(model, "body", string_c_str(body)) && lab_eval_put(model, "wrapped", json_new_bool(wrapped));
    if (!lab_eval_put(model, "bindings", bindings))
        ok = false;
    if (!lab_eval_put(model, "conditions", conditions))
        ok = false;
    string_free(body);
    if (!ok) {
        json_free(model);
        model = NULL;
    }
    return model;
}

static string_t *lab_pres_editor_format(const json_t *model, bool goal)
{
    const json_t *bindings = lab_eval_get(model, "bindings"), *conditions = lab_eval_get(model, "conditions");
    size_t count = json_array_size(bindings);
    const json_t **ordered = count <= 256u ? calloc(count ? count : 1u, sizeof(*ordered)) : NULL;
    string_t *variables = string_new(), *constants = string_new();
    bool ok = variables && constants && ordered;
    if (ordered) {
        size_t variable_count = 0u, constant_count = 0u;
        for (size_t i = 0; i < count; ++i) {
            const json_t *binding = json_array_get(bindings, i);
            if (strcmp(lab_eval_text(binding, "kind"), "constant"))
                ordered[variable_count++] = binding;
        }
        for (size_t i = 0; i < count; ++i) {
            const json_t *binding = json_array_get(bindings, i);
            if (!strcmp(lab_eval_text(binding, "kind"), "constant"))
                ordered[variable_count + constant_count++] = binding;
        }
        qsort(ordered + variable_count, constant_count, sizeof(*ordered), lab_pres_binding_compare);
    }
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *binding = ordered[i];
        bool constant = !strcmp(lab_eval_text(binding, "kind"), "constant");
        const char *value = lab_eval_text(binding, "value");
        string_t *assignment = string_sprintf("%s = %s", lab_eval_text(binding, "name"),
                                              (goal && !constant) || lab_pres_unset(value) ? "?" : value);
        ok = assignment && lab_pres_join(constant ? constants : variables, string_c_str(assignment), ", ");
        string_free(assignment);
    }
    string_t *out = ok ? string_new_with(lab_eval_text(model, "body")) : NULL;
    if (ok && (json_array_size(bindings) || json_array_size(conditions))) {
        string_free(out);
        out = string_sprintf("{ %s | %s", lab_eval_text(model, "body"), string_c_str(variables));
        ok = out && ((!string_byte_length(constants) && !json_array_size(conditions)) ||
                     (!string_append_cstr(out, "; ") && !string_append_string(out, constants)));
        for (size_t i = 0; ok && i < json_array_size(conditions); ++i) {
            const string_t *condition = json_string_value(json_array_get(conditions, i));
            ok = condition && !string_append_cstr(out, "; ") && !string_append_string(out, condition);
        }
        ok = ok && !string_append_cstr(out, " }");
    }
    string_free(constants);
    string_free(variables);
    free(ordered);
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}

/* Analyse authored text without evaluating its values or substituting its bindings. */
json_t *lab_pres_editor_metadata(const string_t *source)
{
    if (!source || strlen(string_c_str(source)) != string_byte_length(source))
        return NULL;
    json_t *model = lab_pres_editor_parse(source);
    json_t *starts = json_new_object();
    string_t *formatted = model ? lab_pres_editor_format(model, false) : NULL;
    string_t *compact = formatted ? lab_pres_compact(formatted) : NULL;
    string_t *goal = model ? lab_pres_editor_format(model, true) : NULL;
    bool ok = model && starts && formatted && compact && goal;
    const json_t *bindings = lab_eval_get(model, "bindings");
    for (size_t i = 0; ok && i < json_array_size(bindings); ++i) {
        const json_t *binding = json_array_get(bindings, i);
        const char *value = lab_eval_text(binding, "value");
        if (strcmp(lab_eval_text(binding, "kind"), "constant") && !lab_pres_unset(value))
            ok = lab_eval_set(starts, lab_eval_text(binding, "name"), value);
    }
    ok = ok && lab_eval_set(model, "text", string_c_str(source)) &&
         lab_eval_set(model, "expression", string_c_str(formatted)) &&
         lab_eval_set(model, "display", string_c_str(compact)) &&
         lab_eval_set(model, "goal_expression", string_c_str(goal)) &&
         lab_eval_put(model, "solved_variables", json_new_bool(json_object_size(starts) > 0u));
    if (!lab_eval_put(model, "starts", starts))
        ok = false;
    string_free(goal);
    string_free(formatted);
    string_free(compact);
    if (!ok) {
        json_free(model);
        model = NULL;
    }
    return model;
}

static bool lab_pres_editor_calculus(json_t *model, const json_t *payload)
{
    const char *action = lab_eval_text(payload, "calculus");
    const string_t *name = json_string_value(lab_eval_get(payload, "name"));
    const char *body = lab_eval_text(model, "body");
    if (!*body || !lab_pres_identifier(name) || (strcmp(action, "derivative") && strcmp(action, "integral")))
        return false;
    string_t *next =
        !strcmp(action, "integral") ? string_sprintf("@S%sd%S", body, name) : string_sprintf("D%S(%s)", name, body);
    bool ok = next && lab_eval_set(model, "body", string_c_str(next));
    string_free(next);
    return ok;
}

/* Apply an explicit authored-value edit, retaining conditions and untouched bindings. */
json_t *lab_pres_editor_action(const json_t *payload, const string_t *text)
{
    const char *operation = lab_eval_text(payload, "operation");
    if (!*operation || !strcmp(operation, "analyse"))
        return lab_pres_editor_metadata(text);
    if (!strcmp(operation, "kind") &&
        (!lab_pres_identifier(json_string_value(lab_eval_get(payload, "name"))) ||
         (strcmp(lab_eval_text(payload, "kind"), "constant") && strcmp(lab_eval_text(payload, "kind"), "variable"))))
        return NULL;
    json_t *model = lab_pres_editor_parse(text);
    if (!model)
        return NULL;
    bool ok = true;
    if (!strcmp(operation, "calculus")) {
        ok = lab_pres_editor_calculus(model, payload);
    } else if (!strcmp(operation, "bindings") || !strcmp(operation, "kind")) {
        json_t *next = json_new_array();
        const json_t *changes = lab_eval_get(payload, "bindings");
        const json_t *old = lab_eval_get(model, "bindings");
        bool replace = !strcmp(lab_eval_text(payload, "mode"), "replace");
        json_t *by_name = json_new_object();
        ok = next && by_name &&
             (!strcmp(operation, "kind") || (json_type(changes) == JSON_ARRAY && json_array_size(changes) <= 256u));
        /* A keyed table prevents quadratic scans when applying a batch of field edits. */
        for (size_t i = 0; ok && i < json_array_size(changes); ++i) {
            const json_t *change = json_array_get(changes, i);
            const char *name = lab_eval_text(change, "name");
            ok = *name && !lab_eval_get(by_name, name) && lab_pres_editor_binding_valid(change) &&
                 lab_eval_put(by_name, name, json_clone(change));
        }
        for (size_t i = 0; ok && !replace && i < json_array_size(old); ++i) {
            const json_t *binding = json_array_get(old, i);
            const char *name = lab_eval_text(binding, "name");
            const json_t *change = lab_eval_get(by_name, name);
            json_t *copy = json_clone(change ? change : binding);
            if (!strcmp(operation, "kind") && !strcmp(name, lab_eval_text(payload, "name")))
                ok = (!strcmp(lab_eval_text(payload, "kind"), "constant") ||
                      !strcmp(lab_eval_text(payload, "kind"), "variable")) &&
                     lab_eval_set(copy, "kind", lab_eval_text(payload, "kind"));
            if (!lab_pres_append(next, copy))
                ok = false;
            if (change)
                ok = ok && lab_eval_put(by_name, name, json_new_null());
        }
        for (size_t i = 0; ok && i < json_array_size(changes); ++i) {
            const json_t *change = json_array_get(changes, i);
            if (json_type(lab_eval_get(by_name, lab_eval_text(change, "name"))) != JSON_NULL)
                ok = json_array_append(next, change);
        }
        json_free(by_name);
        if (!lab_eval_put(model, "bindings", next))
            ok = false;
    } else if (*operation && strcmp(operation, "analyse") && strcmp(operation, "goal_seek")) {
        ok = false;
    }
    string_t *out = ok ? lab_pres_editor_format(model, !strcmp(operation, "goal_seek")) : NULL;
    bool remove_constants = false;
    json_bool_value(lab_eval_get(payload, "unset_constants"), &remove_constants);
    const json_t *changes = lab_eval_get(payload, "bindings");
    for (size_t i = 0; out && remove_constants && i < json_array_size(changes); ++i) {
        const json_t *change = json_array_get(changes, i);
        if (strcmp(lab_eval_text(change, "kind"), "constant") || !lab_pres_unset(lab_eval_text(change, "value")))
            continue;
        const string_t *name = json_string_value(lab_eval_get(change, "name"));
        string_t *reduced = name ? lab_pres_unset_one(out, name) : NULL;
        string_free(out);
        out = reduced;
    }
    json_t *result = out ? lab_pres_editor_metadata(out) : NULL;
    if (result && !strcmp(operation, "goal_seek")) {
        json_t *original = lab_pres_editor_metadata(text);
        if (!original || !lab_eval_put(result, "starts", json_clone(lab_eval_get(original, "starts")))) {
            json_free(result);
            result = NULL;
        }
        json_free(original);
    }
    json_free(model);
    string_free(out);
    return result;
}
