/* xml_write.c - Validated streaming XML output and labelled storage. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"

#define MARS_XML_INTERNAL_ACCESS
#include "xml_internal.h"

enum { XML_WRITE_FRAGMENT_SIZE = 4096 };

typedef struct {
    string_t *fragment;
    xml_write_fn callback;
    void *user_data;
} xml_writer_t;

typedef enum { XML_WRITE_LITERAL, XML_WRITE_TEXT, XML_WRITE_ATTRIBUTE } xml_escape_t;

static bool xml_writer_flush(xml_writer_t *writer)
{
    if (!string_byte_length(writer->fragment))
        return true;
    if (!writer->callback(writer->fragment, writer->user_data))
        return false;
    string_clear(writer->fragment);
    return true;
}

static bool xml_writer_literal(xml_writer_t *writer, const char *literal)
{
    size_t length = strlen(literal);

    if (length > XML_WRITE_FRAGMENT_SIZE)
        return false;
    if (string_byte_length(writer->fragment) > XML_WRITE_FRAGMENT_SIZE - length && !xml_writer_flush(writer))
        return false;
    return string_append_utf8_exact(writer->fragment, literal, length) == 0;
}

/* Encode a non-NUL scalar for exact comparison with its source spelling. XML forbids NUL. */
static size_t xml_writer_encode_scalar(uint32_t value, char encoded[5])
{
    size_t length;

    if (!value || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
        return 0;
    if (value <= 0x7fu) {
        encoded[0] = (char)value;
        length = 1;
    } else if (value <= 0x7ffu) {
        encoded[0] = (char)(0xc0u | (value >> 6));
        encoded[1] = (char)(0x80u | (value & 0x3fu));
        length = 2;
    } else if (value <= 0xffffu) {
        encoded[0] = (char)(0xe0u | (value >> 12));
        encoded[1] = (char)(0x80u | ((value >> 6) & 0x3fu));
        encoded[2] = (char)(0x80u | (value & 0x3fu));
        length = 3;
    } else {
        encoded[0] = (char)(0xf0u | (value >> 18));
        encoded[1] = (char)(0x80u | ((value >> 12) & 0x3fu));
        encoded[2] = (char)(0x80u | ((value >> 6) & 0x3fu));
        encoded[3] = (char)(0x80u | (value & 0x3fu));
        length = 4;
    }
    encoded[length] = '\0';
    return length;
}

static const char *xml_writer_escape(uint32_t value, xml_escape_t mode)
{
    if (mode == XML_WRITE_LITERAL)
        return NULL;
    if (value == '&')
        return "&amp;";
    if (value == '<')
        return "&lt;";
    if (value == '>')
        return "&gt;";
    if (value == '\r')
        return "&#13;";
    if (mode == XML_WRITE_ATTRIBUTE) {
        if (value == '"')
            return "&quot;";
        if (value == '\n')
            return "&#10;";
        if (value == '\t')
            return "&#9;";
    }
    return NULL;
}

static bool xml_writer_text(xml_writer_t *writer, const string_t *text, xml_escape_t mode)
{
    string_cursor_t *cursor;
    bool ok = true;

    if (!text || !(cursor = string_cursor_new(text)))
        return false;
    while (ok && !string_cursor_done(cursor)) {
        string_view_t remaining = string_cursor_view_between(string_cursor_position(cursor),
                                                             string_cursor_end_position(cursor), cursor);
        string_pos_t span;
        uint32_t value;
        const char *escaped;
        char encoded[5];
        size_t length;

        /* Decode one scalar through a cursor view: a grapheme can exceed the fragment limit. */
        if (!string_view_peek_rune_value(remaining, 0, &value, &span) || !span) {
            ok = false;
            break;
        }
        /* The cursor decoder is permissive: reject any non-canonical UTF-8 before escaping it. */
        length = xml_writer_encode_scalar(value, encoded);
        if (!length || span != length ||
            !string_view_equals_literal(string_view_slice(remaining, 0, span), encoded)) {
            ok = false;
            break;
        }
        escaped = xml_writer_escape(value, mode);
        if (escaped) {
            ok = xml_writer_literal(writer, escaped);
        } else {
            if (string_byte_length(writer->fragment) > XML_WRITE_FRAGMENT_SIZE - length)
                ok = xml_writer_flush(writer);
            if (ok)
                ok = string_append_utf8_exact(writer->fragment, encoded, length) == 0;
        }
        if (ok)
            ok = string_cursor_skip(cursor, span);
    }
    string_cursor_free(cursor);
    return ok;
}

static bool xml_writer_attributes(xml_writer_t *writer, const dictionary_t *attributes)
{
    size_t count = attributes ? dictionary_size(attributes) : 0;

    for (size_t i = 0; i < count; ++i) {
        string_t *const *name = dictionary_get_key(attributes, i);
        string_t *const *value = dictionary_get_value(attributes, i);

        if (!name || !value || !xml_writer_literal(writer, " ") ||
            !xml_writer_text(writer, *name, XML_WRITE_LITERAL) || !xml_writer_literal(writer, "=\"") ||
            !xml_writer_text(writer, *value, XML_WRITE_ATTRIBUTE) || !xml_writer_literal(writer, "\""))
            return false;
    }
    return true;
}

static bool xml_writer_node(xml_writer_t *writer, const xml_t *xml, size_t depth);

static bool xml_writer_children(xml_writer_t *writer, const xml_t *xml, size_t depth)
{
    size_t count = xml_child_count(xml);

    for (size_t i = 0; i < count; ++i) {
        const xml_t *child = xml_child_at(xml, i);

        if (!child || child->type == XML_DOCUMENT || !xml_writer_node(writer, child, depth))
            return false;
    }
    return true;
}

static bool xml_writer_node(xml_writer_t *writer, const xml_t *xml, size_t depth)
{
    if (!xml)
        return false;
    switch (xml->type) {
        case XML_DOCUMENT:
            return depth == 0 && xml_writer_children(writer, xml, depth);

        case XML_ELEMENT:
            if (depth >= XML_MAX_DEPTH || !xml_writer_literal(writer, "<") ||
                !xml_writer_text(writer, xml->name, XML_WRITE_LITERAL) ||
                !xml_writer_attributes(writer, xml->attributes))
                return false;
            if (!xml_child_count(xml))
                return xml_writer_literal(writer, "/>");
            return xml_writer_literal(writer, ">") && xml_writer_children(writer, xml, depth + 1u) &&
                   xml_writer_literal(writer, "</") && xml_writer_text(writer, xml->name, XML_WRITE_LITERAL) &&
                   xml_writer_literal(writer, ">");

        case XML_TEXT:
            return xml_writer_text(writer, xml->text, XML_WRITE_TEXT);

        case XML_COMMENT:
            return xml_writer_literal(writer, "<!--") && xml_writer_text(writer, xml->text, XML_WRITE_LITERAL) &&
                   xml_writer_literal(writer, "-->");

        case XML_PROCESSING_INSTRUCTION:
            return xml_writer_literal(writer, "<?") && xml_writer_text(writer, xml->name, XML_WRITE_LITERAL) &&
                   xml->text && (!string_byte_length(xml->text) || xml_writer_literal(writer, " ")) &&
                   xml_writer_text(writer, xml->text, XML_WRITE_LITERAL) && xml_writer_literal(writer, "?>");

        default:
            return false;
    }
}

static bool xml_write_raw(const xml_t *xml, xml_write_fn callback, void *user_data)
{
    xml_writer_t writer = { .fragment = string_new(), .callback = callback, .user_data = user_data };
    bool ok;

    if (!writer.fragment)
        return false;
    ok = xml_writer_node(&writer, xml, 0) && xml_writer_flush(&writer);
    string_free(writer.fragment);
    return ok;
}

static bool xml_writer_accept_event(xml_event_t event, const string_t *name, const string_t *text,
                                    const dictionary_t *attributes, void *user_data)
{
    (void)event;
    (void)name;
    (void)text;
    (void)attributes;
    (void)user_data;
    return true;
}

static bool xml_writer_feed_reader(const string_t *text, void *user_data)
{
    return xml_reader_feed(user_data, string_c_str(text), string_byte_length(text));
}

static bool xml_writer_validate(const xml_t *xml)
{
    const xml_limits_t limits = { .max_bytes = SIZE_MAX, .max_depth = XML_MAX_DEPTH, .max_nodes = SIZE_MAX };
    xml_reader_t *reader;
    bool ok;

    if (!xml || (xml->type != XML_DOCUMENT && xml->type != XML_ELEMENT))
        return false;
    reader = xml_reader_new(&limits, xml_writer_accept_event, NULL);
    if (!reader)
        return false;
    /* Use the raw writer here; calling xml_write would recursively repeat preflight. */
    ok = xml_write_raw(xml, xml_writer_feed_reader, reader) && xml_reader_finish(reader);
    xml_reader_free(reader);
    return ok;
}

/* Validate the complete document before delivering bounded fragments to the sink. */
bool xml_write(const xml_t *xml, xml_write_fn callback, void *user_data)
{
    return callback && xml_writer_validate(xml) && xml_write_raw(xml, callback, user_data);
}

static bool xml_writer_append_string(const string_t *text, void *user_data)
{
    return string_append_utf8_exact(user_data, string_c_str(text), string_byte_length(text)) == 0;
}

/* Collect validated compact XML into an owned string. */
string_t *xml_to_string(const xml_t *xml)
{
    string_t *text = string_new();

    if (!text)
        return NULL;
    if (!xml_write(xml, xml_writer_append_string, text)) {
        string_free(text);
        return NULL;
    }
    return text;
}

static bool xml_writer_write_file(const string_t *text, void *user_data)
{
    return file_write_text(user_data, text);
}

/* Validate before opening the destination; later failures can leave partial output. */
int xml_to_file(const xml_t *xml, const string_t *path)
{
    file_t *file;
    bool ok;

    if (!path || !xml_writer_validate(xml))
        return -1;
    file = file_new(path);
    if (!file)
        return -1;
    ok = file_create_text(file) && xml_write_raw(xml, xml_writer_write_file, file);
    if (!file_close(file))
        ok = false;
    file_free(file);
    return ok ? 0 : -1;
}

/* Produce an owned UTF-8 payload and labels, clearing supplied outputs on failure. */
bool xml_serialize(const xml_t *xml, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *data = NULL;
    size_t length;

    if (out_type)
        *out_type = NULL;
    if (out_encoding)
        *out_encoding = NULL;
    if (out_data)
        *out_data = NULL;
    if (out_len)
        *out_len = 0;
    if (!out_type || !out_encoding || !out_data || !out_len || out_type == out_encoding)
        return false;

    text = xml_to_string(xml);
    if (!text)
        return false;
    length = string_byte_length(text);
    data = malloc(length ? length : 1u);
    type = string_new_with("xml");
    encoding = string_new_with("xml-utf8");
    if (!data || !type || !encoding) {
        free(data);
        string_free(type);
        string_free(encoding);
        string_free(text);
        return false;
    }
    memcpy(data, string_c_str(text), length);
    string_free(text);
    *out_type = type;
    *out_encoding = encoding;
    *out_data = data;
    *out_len = length;
    return true;
}

static bool xml_writer_label_matches(const string_t *label, const char *expected)
{
    string_t *reference;
    bool equal;

    if (!label)
        return false;
    reference = string_new_with(expected);
    if (!reference)
        return false;
    equal = string_compare(label, reference) == 0;
    string_free(reference);
    return equal;
}

/* Check storage labels and parse the exact payload bytes as a complete document. */
xml_t *xml_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    xml_reader_t *reader;
    xml_t *document = NULL;

    if ((!data && len) || !xml_writer_label_matches(type, "xml") ||
        !xml_writer_label_matches(encoding, "xml-utf8"))
        return NULL;
    reader = xml_reader_new(NULL, NULL, NULL);
    if (!reader)
        return NULL;
    if (xml_reader_feed(reader, data, len) && xml_reader_finish(reader))
        document = xml_reader_take_document(reader);
    xml_reader_free(reader);
    return document;
}
