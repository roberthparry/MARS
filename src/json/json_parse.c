/* json_parse.c - JSON text parsing and file loading */

#include <stdint.h>
#include <stdio.h>

#include "json_internal.h"

typedef struct {
    string_cursor_t *cursor;
} json_parser_t;

static void json_skip_ws(json_parser_t *parser)
{
    unsigned char ch;

    while (parser &&
           parser->cursor &&
           string_cursor_peek_ascii(parser->cursor, &ch) &&
           (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
        if (string_cursor_next(parser->cursor) != 0)
            return;
    }
}

static bool json_consume_ascii(json_parser_t *parser, char expected)
{
    rune_t rune;

    if (!parser || !parser->cursor)
        return false;

    rune = string_cursor_peek(parser->cursor);
    if (!rune_is_equal(rune, expected))
        return false;

    return string_cursor_next(parser->cursor) == 0;
}

static bool json_consume_literal(json_parser_t *parser, const char *literal)
{
    if (!parser || !parser->cursor)
        return false;

    return string_cursor_consume(parser->cursor, literal);
}

static int json_hex_value(uint32_t value)
{
    if (value >= '0' && value <= '9')
        return (int)(value - '0');
    if (value >= 'A' && value <= 'F')
        return 10 + (int)(value - 'A');
    if (value >= 'a' && value <= 'f')
        return 10 + (int)(value - 'a');
    return -1;
}

static bool json_append_scalar(string_t *out, uint32_t scalar)
{
    char bytes[4];
    size_t len;

    if (!out ||
        scalar > 0x10FFFFu ||
        (scalar >= 0xD800u && scalar <= 0xDFFFu))
        return false;

    if (scalar <= 0x7Fu) {
        bytes[0] = (char)scalar;
        len = 1u;
    } else if (scalar <= 0x7FFu) {
        bytes[0] = (char)(0xC0u | (scalar >> 6u));
        bytes[1] = (char)(0x80u | (scalar & 0x3Fu));
        len = 2u;
    } else if (scalar <= 0xFFFFu) {
        bytes[0] = (char)(0xE0u | (scalar >> 12u));
        bytes[1] = (char)(0x80u | ((scalar >> 6u) & 0x3Fu));
        bytes[2] = (char)(0x80u | (scalar & 0x3Fu));
        len = 3u;
    } else {
        bytes[0] = (char)(0xF0u | (scalar >> 18u));
        bytes[1] = (char)(0x80u | ((scalar >> 12u) & 0x3Fu));
        bytes[2] = (char)(0x80u | ((scalar >> 6u) & 0x3Fu));
        bytes[3] = (char)(0x80u | (scalar & 0x3Fu));
        len = 4u;
    }

    return string_append_chars(out, bytes, len) == 0;
}

static bool json_parse_hex4(json_parser_t *parser, uint32_t *out)
{
    uint32_t scalar = 0u;

    for (int i = 0; i < 4; ++i) {
        rune_t rune = string_cursor_peek(parser->cursor);
        int digit = json_hex_value(rune_value(rune));

        if (digit < 0)
            return false;
        scalar = (scalar << 4u) | (uint32_t)digit;
        if (string_cursor_next(parser->cursor) != 0)
            return false;
    }

    *out = scalar;
    return true;
}

static bool json_parse_unicode_escape(json_parser_t *parser, string_t *out)
{
    uint32_t scalar;

    if (!json_parse_hex4(parser, &scalar))
        return false;

    if (scalar >= 0xD800u && scalar <= 0xDBFFu) {
        uint32_t low;

        if (!json_consume_ascii(parser, '\\') ||
            !json_consume_ascii(parser, 'u') ||
            !json_parse_hex4(parser, &low))
            return false;

        if (low < 0xDC00u || low > 0xDFFFu)
            return false;

        scalar = 0x10000u +
                 ((scalar - 0xD800u) << 10u) +
                 (low - 0xDC00u);
    } else if (scalar >= 0xDC00u && scalar <= 0xDFFFu) {
        return false;
    }

    return json_append_scalar(out, scalar);
}

static string_t *json_parse_string_text(json_parser_t *parser)
{
    string_t *text;
    bool closed = false;

    if (!json_consume_ascii(parser, '"'))
        return NULL;

    text = string_new();
    if (!text)
        return NULL;

    while (parser && parser->cursor && !string_cursor_done(parser->cursor)) {
        rune_t rune = string_cursor_peek(parser->cursor);
        unsigned char ascii;

        if (rune_is_none(rune))
            goto fail;

        if (!string_cursor_peek_ascii(parser->cursor, &ascii)) {
            if (string_append_rune(text, rune) != 0 ||
                string_cursor_next(parser->cursor) != 0)
                goto fail;
            continue;
        }

        if (ascii == '"') {
            if (string_cursor_next(parser->cursor) != 0)
                goto fail;
            closed = true;
            break;
        }

        if (ascii == '\\') {
            if (!json_consume_ascii(parser, '\\'))
                goto fail;
            if (!string_cursor_peek_ascii(parser->cursor, &ascii))
                goto fail;
            if (string_cursor_next(parser->cursor) != 0)
                goto fail;

            switch (ascii) {
            case '"':
            case '\\':
            case '/':
                if (string_append_char(text, (char)ascii) != 0)
                    goto fail;
                break;
            case 'b':
                if (string_append_char(text, '\b') != 0)
                    goto fail;
                break;
            case 'f':
                if (string_append_char(text, '\f') != 0)
                    goto fail;
                break;
            case 'n':
                if (string_append_char(text, '\n') != 0)
                    goto fail;
                break;
            case 'r':
                if (string_append_char(text, '\r') != 0)
                    goto fail;
                break;
            case 't':
                if (string_append_char(text, '\t') != 0)
                    goto fail;
                break;
            case 'u':
                if (!json_parse_unicode_escape(parser, text))
                    goto fail;
                break;
            default:
                goto fail;
            }
            continue;
        }

        if (ascii < 0x20u)
            goto fail;

        if (string_append_char(text, (char)ascii) != 0 ||
            string_cursor_next(parser->cursor) != 0)
            goto fail;
    }

    if (!closed)
        goto fail;

    return text;

fail:
    string_free(text);
    return NULL;
}

static json_t *json_parse_value(json_parser_t *parser);

static json_t *json_parse_string_value(json_parser_t *parser)
{
    string_t *text = json_parse_string_text(parser);
    json_t *json;

    if (!text)
        return NULL;

    json = json_new_string(text);
    string_free(text);
    return json;
}

static bool json_parse_number_scan(json_parser_t *parser, string_pos_t *start_out, string_pos_t *end_out)
{
    string_cursor_t *cursor;
    string_pos_t start;
    unsigned char ch;
    bool ok = false;

    if (!parser || !parser->cursor)
        return false;

    cursor = parser->cursor;
    start = string_cursor_position(cursor);

    if (string_cursor_peek_ascii(cursor, &ch) && ch == '-')
        (void)string_cursor_next(cursor);

    if (!string_cursor_peek_ascii(cursor, &ch))
        goto done;

    if (ch == '0') {
        (void)string_cursor_next(cursor);
    } else if (ch >= '1' && ch <= '9') {
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) &&
                 ch >= '0' && ch <= '9');
    } else {
        goto done;
    }

    if (string_cursor_peek_ascii(cursor, &ch) && ch == '.') {
        (void)string_cursor_next(cursor);
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9')
            goto done;
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) &&
                 ch >= '0' && ch <= '9');
    }

    if (string_cursor_peek_ascii(cursor, &ch) && (ch == 'e' || ch == 'E')) {
        (void)string_cursor_next(cursor);
        if (string_cursor_peek_ascii(cursor, &ch) && (ch == '+' || ch == '-'))
            (void)string_cursor_next(cursor);
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9')
            goto done;
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) &&
                 ch >= '0' && ch <= '9');
    }

    ok = true;
    *start_out = start;
    *end_out = string_cursor_position(cursor);

done:
    if (!ok)
        (void)string_cursor_seek(cursor, start);
    return ok;
}

static json_t *json_parse_number_value(json_parser_t *parser)
{
    string_pos_t start;
    string_pos_t end;
    string_t *number_text;
    json_t *json;

    if (!json_parse_number_scan(parser, &start, &end))
        return NULL;

    number_text = string_cursor_slice_between(start, end, parser->cursor);
    if (!number_text)
        return NULL;

    json = json_new_number(number_text);
    string_free(number_text);
    return json;
}

static json_t *json_maybe_unwrap_extended_number(json_t *object)
{
    const string_t *key;
    const json_t *value;
    string_t *text;
    json_t *number_json;
    number_t number_value;

    if (!object ||
        object->type != JSON_OBJECT ||
        json_object_size(object) != 1u)
        return object;

    key = json_object_key_at(object, 0u);
    if (!key ||
        !string_view_equals_literal(string_view_all(key), "$mars.number"))
        return object;

    value = json_object_value_at(object, 0u);
    if (!value || value->type != JSON_STRING)
        return object;

    if (!json_number_value_from_spelling(value->u.text, &number_value))
        return object;

    text = json_number_spelling(number_value);
    if (!text) {
        num_destroy(&number_value);
        return object;
    }

    number_json = json_alloc(JSON_NUMBER);
    if (!number_json) {
        string_free(text);
        num_destroy(&number_value);
        return object;
    }

    number_json->u.number.text = text;
    number_json->u.number.value = number_value;
    json_free(object);
    return number_json;
}

static json_t *json_parse_array_value(json_parser_t *parser)
{
    json_t *array;

    if (!json_consume_ascii(parser, '['))
        return NULL;

    array = json_new_array();
    if (!array)
        return NULL;

    json_skip_ws(parser);
    if (json_consume_ascii(parser, ']'))
        return array;

    while (true) {
        json_t *item;

        json_skip_ws(parser);
        item = json_parse_value(parser);
        if (!item)
            goto fail;
        if (!json_array_append(array, item)) {
            json_free(item);
            goto fail;
        }
        json_free(item);

        json_skip_ws(parser);
        if (json_consume_ascii(parser, ']'))
            return array;
        if (!json_consume_ascii(parser, ','))
            goto fail;
    }

fail:
    json_free(array);
    return NULL;
}

static json_t *json_parse_object_value(json_parser_t *parser)
{
    json_t *object;

    if (!json_consume_ascii(parser, '{'))
        return NULL;

    object = json_new_object();
    if (!object)
        return NULL;

    json_skip_ws(parser);
    if (json_consume_ascii(parser, '}'))
        return object;

    while (true) {
        string_t *key;
        json_t *value;

        json_skip_ws(parser);
        key = json_parse_string_text(parser);
        if (!key)
            goto fail;

        json_skip_ws(parser);
        if (!json_consume_ascii(parser, ':')) {
            string_free(key);
            goto fail;
        }

        json_skip_ws(parser);
        value = json_parse_value(parser);
        if (!value) {
            string_free(key);
            goto fail;
        }

        if (!json_object_set(object, key, value)) {
            json_free(value);
            string_free(key);
            goto fail;
        }
        json_free(value);
        string_free(key);

        json_skip_ws(parser);
        if (json_consume_ascii(parser, '}'))
            return json_maybe_unwrap_extended_number(object);
        if (!json_consume_ascii(parser, ','))
            goto fail;
    }

fail:
    json_free(object);
    return NULL;
}

static json_t *json_parse_value(json_parser_t *parser)
{
    unsigned char ch;

    json_skip_ws(parser);
    if (!parser || !parser->cursor || string_cursor_done(parser->cursor))
        return NULL;

    if (!string_cursor_peek_ascii(parser->cursor, &ch))
        return NULL;

    switch (ch) {
    case 'n':
        return json_consume_literal(parser, "null") ? json_new_null() : NULL;
    case 't':
        return json_consume_literal(parser, "true") ? json_new_bool(true) : NULL;
    case 'f':
        return json_consume_literal(parser, "false") ? json_new_bool(false) : NULL;
    case '"':
        return json_parse_string_value(parser);
    case '[':
        return json_parse_array_value(parser);
    case '{':
        return json_parse_object_value(parser);
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return json_parse_number_value(parser);
    default:
        return NULL;
    }
}

json_t *json_from_string(const string_t *text)
{
    json_parser_t parser;
    json_t *json;

    if (!text)
        return NULL;

    parser.cursor = string_cursor_new(text);
    if (!parser.cursor)
        return NULL;

    json = json_parse_value(&parser);
    if (json) {
        json_skip_ws(&parser);
        if (!string_cursor_done(parser.cursor)) {
            json_free(json);
            json = NULL;
        }
    }

    string_cursor_free(parser.cursor);
    return json;
}

json_t *json_from_file(const string_t *path)
{
    FILE *file;
    string_t *text;
    char buffer[4096];
    json_t *json;

    if (!path)
        return NULL;

    file = fopen(string_c_str(path), "rb");
    if (!file)
        return NULL;

    text = string_new();
    if (!text) {
        fclose(file);
        return NULL;
    }

    while (true) {
        size_t got = fread(buffer, 1u, sizeof(buffer), file);

        if (got > 0u && string_append_chars(text, buffer, got) != 0) {
            string_free(text);
            fclose(file);
            return NULL;
        }
        if (got < sizeof(buffer)) {
            if (ferror(file)) {
                string_free(text);
                fclose(file);
                return NULL;
            }
            break;
        }
    }

    fclose(file);
    json = json_from_string(text);
    string_free(text);
    return json;
}
