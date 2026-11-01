/**
 * @file expr_notation_quantum.c
 * @brief Recognise notation-only operator and Dirac constructs in native mathematical input.
 *
 * Expression owns ASCII name recognition and selects an order-preserving parse for explicit quantum notation.
 * A compact perfect hash supplies semantic node kinds and arities; TeX alone supplies their typesetting syntax.
 * These helpers do not evaluate operators, states, inner products or adjoints.
 */
#include <string.h>

#include "string-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringin_scan.h"

/* Recognise the fixed vocabulary without scanning the table. */
TeX_node_kind_t expr_notation_quantum_function(const char *name, size_t *arity)
{
    static const struct {
        const char *name;
        TeX_node_kind_t kind;
        size_t arity;
    } names[10] = {
        [0] = {"expect",         TeX_NODE_FENCED,         1u},
        [2] = {"braket",         TeX_NODE_BRAKET,         2u},
        [3] = {"ket",            TeX_NODE_KET,            1u},
        [4] = {"ketbra",         TeX_NODE_KETBRA,         2u},
        [5] = {"hat",            TeX_NODE_HAT,            1u},
        [6] = {"matrix_element", TeX_NODE_MATRIX_ELEMENT, 3u},
        [7] = {"bra",            TeX_NODE_BRA,            1u},
        [8] = {"dagger",         TeX_NODE_DAGGER,         1u}
    };
    size_t length = name ? strlen(name) : 0u;
    if (!length)
        return TeX_NODE_COUNT;
    size_t slot = (length + 6u * (unsigned char)name[0] + 8u * (unsigned char)name[length - 1u]) % 10u;
    if (!names[slot].name || strcmp(name, names[slot].name))
        return TeX_NODE_COUNT;
    if (arity)
        *arity = names[slot].arity;
    return names[slot].kind;
}

/* Bound the probe by the longest reserved function spelling, including its terminating NUL. */
string_t *expr_notation_read_quantum_function(string_cursor_t *cursor)
{
    string_cursor_t *scan = cursor ? string_cursor_clone(cursor) : NULL;
    char name[sizeof "matrix_element"];
    size_t length = 0u;
    unsigned char c;
    while (scan && string_cursor_peek_ascii(scan, &c) && ((c >= 'a' && c <= 'z') || c == '_') &&
           length + 1u < sizeof name) {
        name[length++] = (char)c;
        string_cursor_skip(scan, 1u);
    }
    name[length] = '\0';
    string_pos_t end = scan ? string_cursor_position(scan) : 0u;
    if (scan)
        string_cursor_skip_spaces(scan);
    bool found = scan && string_cursor_peek_ascii(scan, &c) && c == '(' &&
                 expr_notation_quantum_function(name, NULL) != TeX_NODE_COUNT;
    string_t *result = found ? string_new_with(name) : NULL;
    if (result)
        string_cursor_seek(cursor, end);
    string_cursor_free(scan);
    return result;
}

/* Scan tokens once; a function name alone is still a name, and a suffix must end at a name boundary. */
bool expr_notation_quantum_input(const string_t *text)
{
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    bool found = false;
    while (cursor && !found && !string_cursor_done(cursor)) {
        string_t *function = expr_notation_read_quantum_function(cursor);
        if (function) {
            string_free(function);
            found = true;
            break;
        }
        string_t *name = expr_parse_read_name(cursor, true);
        if (name) {
            const char *plain = string_c_str(name);
            size_t length = string_byte_length(name);
            string_pos_t position = string_cursor_position(cursor);
            found = (length > 3u && !strcmp(plain + length - 3u, "_op")) ||
                (string_cursor_match_at(cursor, position, "_op") &&
                 expr_parse_cursor_at_identifier_boundary(cursor, position + 3u));
            string_cursor_skip_spaces(cursor);
            unsigned char c;
            found = found || (string_cursor_peek_ascii(cursor, &c) && c == '(' &&
                             expr_notation_quantum_function(plain, NULL) != TeX_NODE_COUNT);
            string_free(name);
        } else if (string_cursor_next(cursor)) {
            break;
        }
    }
    string_cursor_free(cursor);
    return found;
}
