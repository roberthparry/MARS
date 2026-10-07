/**
 * @file xml_parse.c
 * @brief Native incremental XML parsing.
 *
 * Consumes bounded UTF-8 chunks, builds trees or emits reader events and reports malformed input. Unsupported
 * external entities and document-type features are rejected rather than fetched.
 *
 * This is part of xml.h's native implementation. Keep input bounded and reject unsupported external-entity
 * features; HTTP and SOAP transport belong to the HTTP module.
 */

/* xml_parse.c - bounded incremental XML 1.0 parsing over MARS strings. */
#include <stdint.h>
#include <stdlib.h>
#define MARS_XML_INTERNAL_ACCESS
#include "xml_internal.h"
#include "file.h"

#include "xml_parse_private.h"

bool xml_parse_fail(xml_reader_t *reader, const char *message)
{
    if (!reader->failed) {
        reader->failed = true;
        reader->error = string_new_with(message);
    }
    return false;
}

static bool parse_space(uint32_t c)
{
    return c == 0x20 || c == 9 || c == 10 || c == 13;
}

static bool parse_character(uint32_t c)
{
    return c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0xd7ff) ||
           (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
}

bool xml_parse_name_start(uint32_t c)
{
    return c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) || (c >= 0xf8 && c <= 0x2ff) ||
           (c >= 0x370 && c <= 0x37d) || (c >= 0x37f && c <= 0x1fff) || (c >= 0x200c && c <= 0x200d) ||
           (c >= 0x2070 && c <= 0x218f) || (c >= 0x2c00 && c <= 0x2fef) || (c >= 0x3001 && c <= 0xd7ff) ||
           (c >= 0xf900 && c <= 0xfdcf) || (c >= 0xfdf0 && c <= 0xfffd) || (c >= 0x10000 && c <= 0xeffff);
}

bool xml_parse_name_char(uint32_t c)
{
    return xml_parse_name_start(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xb7 ||
           (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040);
}

/* XML grammar is defined on scalars, not the string module's user-visible graphemes. */
uint32_t xml_parse_peek(const parse_cursor_t *input)
{
    uint32_t value = 0;
    string_view_peek_rune_value(input->view, string_cursor_position(input->cursor), &value, NULL);
    return value;
}

void xml_parse_next(parse_cursor_t *input)
{
    string_pos_t next;
    if (string_view_peek_rune_value(input->view, string_cursor_position(input->cursor), NULL, &next))
        string_cursor_seek(input->cursor, next);
}

static bool parse_spaces(parse_cursor_t *input)
{
    bool any = false;
    while (parse_space(xml_parse_peek(input))) {
        any = true;
        xml_parse_next(input);
    }
    return any;
}

bool xml_parse_literal(const string_t *text, const char *literal)
{
    return string_view_equals_literal(string_view_all(text), literal);
}

static string_t *parse_name(xml_reader_t *reader, parse_cursor_t *input)
{
    if (!xml_parse_name_start(xml_parse_peek(input))) {
        xml_parse_fail(reader, "Expected an XML name");
        return NULL;
    }
    string_pos_t start = string_cursor_position(input->cursor);
    do {
        xml_parse_next(input);
    } while (xml_parse_name_char(xml_parse_peek(input)));
    string_t *name = string_cursor_extract(start, input->cursor);
    if (!name)
        xml_parse_fail(reader, "Cannot allocate XML name");
    return name;
}

static bool parse_append(xml_reader_t *reader, string_t *out, uint32_t value)
{
    /* Encode an already validated scalar at the string-storage boundary, preserving its exact identity. */
    char bytes[4];
    size_t count;
    if (value <= 0x7f) {
        bytes[0] = (char)value;
        count = 1;
    } else if (value <= 0x7ff) {
        bytes[0] = (char)(0xc0 | (value >> 6));
        bytes[1] = (char)(0x80 | (value & 0x3f));
        count = 2;
    } else if (value <= 0xffff) {
        bytes[0] = (char)(0xe0 | (value >> 12));
        bytes[1] = (char)(0x80 | ((value >> 6) & 0x3f));
        bytes[2] = (char)(0x80 | (value & 0x3f));
        count = 3;
    } else {
        bytes[0] = (char)(0xf0 | (value >> 18));
        bytes[1] = (char)(0x80 | ((value >> 12) & 0x3f));
        bytes[2] = (char)(0x80 | ((value >> 6) & 0x3f));
        bytes[3] = (char)(0x80 | (value & 0x3f));
        count = 4;
    }
    return string_append_utf8_exact(out, bytes, count) == 0 || xml_parse_fail(reader, "Cannot allocate XML text");
}

static bool parse_reference(xml_reader_t *reader, parse_cursor_t *input, string_t *out)
{
    string_cursor_consume(input->cursor, "&");
    uint32_t value = 0;
    if (string_cursor_consume(input->cursor, "#")) {
        unsigned base = string_cursor_consume(input->cursor, "x") ? 16 : 10;
        size_t digits = 0;
        while (xml_parse_peek(input) != ';') {
            uint32_t c = xml_parse_peek(input);
            unsigned digit;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (base == 16 && c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else if (base == 16 && c >= 'A' && c <= 'F')
                digit = c - 'A' + 10;
            else
                return xml_parse_fail(reader, "Invalid numeric character reference");
            if (value > (0x10ffffu - digit) / base)
                return xml_parse_fail(reader, "Character reference is out of range");
            value = value * base + digit;
            ++digits;
            xml_parse_next(input);
        }
        if (!digits || !parse_character(value))
            return xml_parse_fail(reader, "Forbidden XML character reference");
        xml_parse_next(input);
    } else if (string_cursor_consume(input->cursor, "amp;")) {
        value = '&';
    } else if (string_cursor_consume(input->cursor, "lt;")) {
        value = '<';
    } else if (string_cursor_consume(input->cursor, "gt;")) {
        value = '>';
    } else if (string_cursor_consume(input->cursor, "apos;")) {
        value = '\'';
    } else if (string_cursor_consume(input->cursor, "quot;")) {
        value = '"';
    } else {
        return xml_parse_fail(reader, "Unknown or unterminated entity reference; DTD entities are forbidden");
    }
    return parse_append(reader, out, value);
}

static string_t *parse_quoted(xml_reader_t *reader, parse_cursor_t *input, bool declaration)
{
    uint32_t quote = xml_parse_peek(input);
    if (quote != '\'' && quote != '"') {
        xml_parse_fail(reader, "Expected a quoted attribute value");
        return NULL;
    }
    xml_parse_next(input);
    string_t *value = string_new();
    if (!value) {
        xml_parse_fail(reader, "Cannot allocate attribute value");
        return NULL;
    }
    while (xml_parse_peek(input) != quote) {
        uint32_t c = xml_parse_peek(input);
        if (!c || c == '<' || (declaration && c == '&')) {
            xml_parse_fail(reader, "Invalid or unterminated attribute value");
            break;
        }
        if (c == '&') {
            if (!parse_reference(reader, input, value))
                break;
        } else {
            if (!parse_append(reader, value, parse_space(c) ? ' ' : c))
                break;
            xml_parse_next(input);
        }
    }
    if (reader->failed) {
        string_free(value);
        return NULL;
    }
    xml_parse_next(input);
    return value;
}

static bool parse_event(xml_reader_t *reader, xml_event_t event, const string_t *name, const string_t *text,
                        const dictionary_t *attributes)
{
    return !reader->callback || reader->callback(event, name, text, attributes, reader->user_data) ||
           xml_parse_fail(reader, "XML event callback stopped parsing");
}

static bool parse_count(xml_reader_t *reader)
{
    if (reader->nodes >= reader->limits.max_nodes)
        return xml_parse_fail(reader, "XML node limit exceeded");
    ++reader->nodes;
    return true;
}

static xml_t *parse_parent(xml_reader_t *reader)
{
    return reader->depth ? reader->frames[reader->depth - 1].node : reader->document;
}

static bool parse_attach(xml_reader_t *reader, xml_t *node)
{
    if (node && xml_attach_owned(parse_parent(reader), node))
        return true;
    xml_free(node);
    return xml_parse_fail(reader, "Cannot attach XML node");
}

static bool parse_text_event(xml_reader_t *reader, const string_t *text)
{
    if (!string_byte_length(text))
        return true;
    if (!parse_event(reader, XML_EVENT_TEXT, NULL, text, NULL))
        return false;
    if (reader->callback)
        return true;
    xml_t *parent = parse_parent(reader);
    size_t count = xml_child_count(parent);
    xml_t *last = count ? *(xml_t *const *)array_get(parent->children, count - 1) : NULL;
    if (last && last->type == XML_TEXT)
        return string_append_utf8_exact(last->text, string_c_str(text), string_byte_length(text)) == 0 ||
               xml_parse_fail(reader, "Cannot extend XML text");
    return parse_attach(reader, xml_new_text(text));
}

static bool parse_text_token(xml_reader_t *reader)
{
    parse_cursor_t input = {string_cursor_new(reader->token), string_view_all(reader->token)};
    string_t *text = string_new();
    if (!input.cursor || !text)
        xml_parse_fail(reader, "Cannot allocate text parser");
    while (!reader->failed && !string_cursor_done(input.cursor)) {
        uint32_t c = xml_parse_peek(&input);
        if (!reader->depth) {
            if (!parse_space(c))
                xml_parse_fail(reader, "Character data outside the document element");
            xml_parse_next(&input);
        } else if (c == '&') {
            parse_reference(reader, &input, text);
        } else {
            parse_append(reader, text, c);
            xml_parse_next(&input);
        }
    }
    if (!reader->failed && reader->depth)
        parse_text_event(reader, text);
    string_cursor_free(input.cursor);
    string_free(text);
    string_clear(reader->token);
    return !reader->failed;
}

const string_t *xml_parse_lookup(const dictionary_t *dict, const string_t *key)
{
    dictionary_entry_t *entry = NULL;
    return dictionary_get_entry(dict, &key, &entry) ? *(string_t *const *)dictionary_entry_value(entry) : NULL;
}

static void parse_pop(xml_reader_t *reader)
{
    parse_frame_t *frame = &reader->frames[--reader->depth];
    string_free(frame->name);
    xml_attribute_storage_free(frame->namespaces);
    *frame = (parse_frame_t){0};
}

static bool parse_start_tag(xml_reader_t *reader, parse_cursor_t *input)
{
    string_t *name = parse_name(reader, input);
    dictionary_t *attributes = xml_attribute_storage_new();
    bool empty = false;
    if (!attributes)
        xml_parse_fail(reader, "Cannot allocate attributes");
    while (!reader->failed) {
        bool separated = parse_spaces(input);
        if (string_cursor_consume(input->cursor, "/>")) {
            empty = true;
            break;
        }
        if (string_cursor_consume(input->cursor, ">"))
            break;
        if (!separated) {
            xml_parse_fail(reader, "Attributes must be separated by XML whitespace");
            break;
        }
        string_t *key = parse_name(reader, input), *value = NULL;
        if (key) {
            parse_spaces(input);
            if (!string_cursor_consume(input->cursor, "="))
                xml_parse_fail(reader, "Expected equals after attribute name");
            else {
                parse_spaces(input);
                value = parse_quoted(reader, input, false);
            }
        }
        if (key && value) {
            if (xml_parse_lookup(attributes, key))
                xml_parse_fail(reader, "Duplicate attribute name");
            else if (!xml_attribute_store(attributes, key, value))
                xml_parse_fail(reader, "Cannot store XML attribute");
        }
        string_free(key);
        string_free(value);
    }
    if (!reader->failed && !string_cursor_done(input->cursor))
        xml_parse_fail(reader, "Unexpected data after start tag");
    if (!reader->failed && reader->depth >= reader->limits.max_depth)
        xml_parse_fail(reader, "XML depth limit exceeded");
    if (!reader->failed && !reader->depth && reader->root_seen)
        xml_parse_fail(reader, "A document must have exactly one root element");
    if (!reader->failed)
        parse_count(reader);
    xml_t *parent = parse_parent(reader);
    if (!reader->failed) {
        parse_frame_t *frame = &reader->frames[reader->depth++];
        frame->name = name;
        name = NULL;
        frame->namespaces = xml_attribute_storage_new();
        if (!frame->namespaces)
            xml_parse_fail(reader, "Cannot allocate namespace scope");
        else
            xml_parse_namespaces(reader, attributes);
        if (!reader->failed && !reader->callback) {
            xml_t *node = xml_new_element(frame->name);
            if (!node || !xml_attach_owned(parent, node)) {
                xml_free(node);
                xml_parse_fail(reader, "Cannot allocate or attach element");
            } else {
                frame->node = node;
                xml_attribute_storage_free(node->attributes);
                node->attributes = attributes;
                attributes = NULL;
            }
        }
        if (!reader->failed) {
            reader->root_seen = true;
            parse_event(reader, XML_EVENT_START, frame->name, NULL,
                        reader->callback ? attributes : frame->node->attributes);
        }
        if (!reader->failed && empty) {
            parse_event(reader, XML_EVENT_END, frame->name, NULL, NULL);
            parse_pop(reader);
        }
    }
    string_free(name);
    xml_attribute_storage_free(attributes);
    return !reader->failed;
}

static bool parse_end_tag(xml_reader_t *reader, parse_cursor_t *input)
{
    string_t *name = parse_name(reader, input);
    if (name) {
        parse_spaces(input);
        if (!string_cursor_consume(input->cursor, ">") || !string_cursor_done(input->cursor))
            xml_parse_fail(reader, "Invalid end tag");
        else if (!reader->depth || string_compare(name, reader->frames[reader->depth - 1].name))
            xml_parse_fail(reader, "Mismatched end tag");
        else if (parse_event(reader, XML_EVENT_END, name, NULL, NULL))
            parse_pop(reader);
    }
    string_free(name);
    return !reader->failed;
}

static bool parse_declaration(xml_reader_t *reader, parse_cursor_t *input)
{
    unsigned stage = 0;
    while (!reader->failed) {
        bool separated = parse_spaces(input);
        if (string_cursor_consume(input->cursor, "?>")) {
            if (!stage)
                xml_parse_fail(reader, "XML declaration requires version 1.0");
            break;
        }
        if (!separated) {
            xml_parse_fail(reader, "XML declaration fields require whitespace");
            break;
        }
        string_t *key = parse_name(reader, input), *value = NULL;
        if (key) {
            parse_spaces(input);
            if (!string_cursor_consume(input->cursor, "="))
                xml_parse_fail(reader, "Expected equals in XML declaration");
            else {
                parse_spaces(input);
                value = parse_quoted(reader, input, true);
            }
        }
        if (key && value) {
            if (!stage && xml_parse_literal(key, "version") && xml_parse_literal(value, "1.0")) {
                stage = 1;
            } else if (stage == 1 && xml_parse_literal(key, "encoding")) {
                string_to_upper(value);
                if (!xml_parse_literal(value, "UTF-8"))
                    xml_parse_fail(reader, "Only UTF-8 XML input is supported");
                stage = 2;
            } else if ((stage == 1 || stage == 2) && xml_parse_literal(key, "standalone") &&
                       (xml_parse_literal(value, "yes") || xml_parse_literal(value, "no"))) {
                stage = 3;
            } else {
                xml_parse_fail(reader, "Invalid XML declaration field, order or version");
            }
        }
        string_free(key);
        string_free(value);
    }
    return !reader->failed;
}

static bool parse_instruction(xml_reader_t *reader, parse_cursor_t *input)
{
    string_t *name = parse_name(reader, input), *text = NULL, *folded = name ? string_clone(name) : NULL;
    if (!name || !folded)
        xml_parse_fail(reader, "Cannot parse processing-instruction target");
    /* Namespace well-formedness requires non-element/attribute names to be NCNames. */
    if (!reader->failed && string_find(name, ":") >= 0)
        xml_parse_fail(reader, "A processing-instruction target must not contain a colon");
    if (!reader->failed) {
        string_to_lower(folded);
        if (xml_parse_literal(folded, "xml")) {
            if (!xml_parse_literal(name, "xml") || !reader->declaration_allowed)
                xml_parse_fail(reader, "Reserved XML target or misplaced XML declaration");
            else
                parse_declaration(reader, input);
        } else {
            bool separated = parse_spaces(input);
            if (!separated && !string_cursor_match(input->cursor, "?>"))
                xml_parse_fail(reader, "Processing-instruction content requires whitespace");
            string_pos_t start = string_cursor_position(input->cursor);
            while (!reader->failed && !string_cursor_done(input->cursor) &&
                   !string_cursor_match(input->cursor, "?>"))
                xml_parse_next(input);
            text = string_cursor_extract(start, input->cursor);
            if (!text || !string_cursor_consume(input->cursor, "?>"))
                xml_parse_fail(reader, "Unterminated processing instruction");
            if (!reader->failed && parse_count(reader) && parse_event(reader, XML_EVENT_PI, name, text, NULL) &&
                !reader->callback)
                parse_attach(reader, xml_new_processing_instruction(name, text));
        }
    }
    string_free(name);
    string_free(folded);
    string_free(text);
    return !reader->failed;
}

static bool parse_delimited(xml_reader_t *reader, parse_cursor_t *input, bool comment)
{
    const char *end = comment ? "-->" : "]]>";
    string_pos_t start = string_cursor_position(input->cursor);
    while (!string_cursor_done(input->cursor) && !string_cursor_match(input->cursor, end)) {
        if (comment && string_cursor_match(input->cursor, "--"))
            return xml_parse_fail(reader, "Double hyphen is forbidden inside XML comments");
        xml_parse_next(input);
    }
    string_t *text = string_cursor_extract(start, input->cursor);
    if (!text || !string_cursor_consume(input->cursor, end))
        xml_parse_fail(reader, "Unterminated XML comment or CDATA section");
    if (!reader->failed && comment) {
        if (parse_count(reader) && parse_event(reader, XML_EVENT_COMMENT, NULL, text, NULL) && !reader->callback)
            parse_attach(reader, xml_new_comment(text));
    } else if (!reader->failed) {
        if (!reader->depth)
            xml_parse_fail(reader, "CDATA is only permitted inside an element");
        else
            parse_text_event(reader, text);
    }
    string_free(text);
    return !reader->failed;
}

static bool parse_markup(xml_reader_t *reader)
{
    parse_cursor_t input = {string_cursor_new(reader->token), string_view_all(reader->token)};
    if (!input.cursor)
        return xml_parse_fail(reader, "Cannot allocate markup parser");
    if (string_cursor_consume(input.cursor, "<!--"))
        parse_delimited(reader, &input, true);
    else if (string_cursor_consume(input.cursor, "<![CDATA["))
        parse_delimited(reader, &input, false);
    else if (string_cursor_consume(input.cursor, "<?"))
        parse_instruction(reader, &input);
    else if (string_cursor_consume(input.cursor, "</"))
        parse_end_tag(reader, &input);
    else if (string_cursor_consume(input.cursor, "<"))
        parse_start_tag(reader, &input);
    else
        xml_parse_fail(reader, "Invalid XML markup");
    if (!reader->failed && !string_cursor_done(input.cursor))
        xml_parse_fail(reader, "Unexpected trailing markup");
    string_cursor_free(input.cursor);
    string_clear(reader->token);
    reader->mode = PARSE_TEXT;
    reader->declaration_allowed = false;
    reader->brackets = 0;
    return !reader->failed;
}

/* Tokenisation consumes decoded scalar values; no XML syntax is examined as raw bytes. */
static bool parse_scalar(xml_reader_t *reader, uint32_t c)
{
    if (reader->first_scalar) {
        reader->first_scalar = false;
        if (c == 0xfeff)
            return true;
    }
    if (!parse_character(c))
        return xml_parse_fail(reader, "Forbidden XML 1.0 character");
    if (reader->previous_cr) {
        reader->previous_cr = false;
        if (c == 10)
            return true;
    }
    if (c == 13) {
        reader->previous_cr = true;
        c = 10;
    }
    if (reader->mode == PARSE_TEXT) {
        if (c == '<') {
            if (!parse_text_token(reader))
                return false;
            reader->mode = PARSE_OPEN;
            reader->brackets = 0;
        } else {
            reader->declaration_allowed = false;
            if (c == '>' && reader->brackets == 2)
                return xml_parse_fail(reader, "The sequence ]]> is forbidden in ordinary character data");
            reader->brackets = c == ']' ? (reader->brackets < 2 ? reader->brackets + 1 : 2) : 0;
            if (c == '&')
                reader->reference = true;
            else if (c == ';')
                reader->reference = false;
            if (!parse_append(reader, reader->token, c))
                return false;
            if (string_byte_length(reader->token) >= 4096 && !reader->reference)
                return parse_text_token(reader);
            return true;
        }
    }
    if (!parse_append(reader, reader->token, c))
        return false;
    if (reader->mode == PARSE_OPEN) {
        size_t length = string_byte_length(reader->token);
        if (length == 1)
            return true;
        if (length == 2 && c == '?') {
            reader->mode = PARSE_PI;
            return true;
        }
        if (string_starts_with(reader->token, "<!")) {
            static const char *const cdata_prefixes[] = {
                "<!", "<![", "<![C", "<![CD", "<![CDA", "<![CDAT", "<![CDATA", "<![CDATA["
            };
            if (xml_parse_literal(reader->token, "<!--"))
                reader->mode = PARSE_COMMENT;
            else if (xml_parse_literal(reader->token, "<![CDATA["))
                reader->mode = PARSE_CDATA;
            else if (!xml_parse_literal(reader->token, "<!-") &&
                     !(length >= 2 && length <= 9 && xml_parse_literal(reader->token, cdata_prefixes[length - 2])))
                return xml_parse_fail(reader, "DTDs and other declarations are forbidden");
            return true;
        }
        reader->mode = PARSE_TAG;
    }
    if (reader->mode == PARSE_TAG) {
        if (reader->quote) {
            if (c == reader->quote)
                reader->quote = 0;
        } else if (c == '\'' || c == '"') {
            reader->quote = c;
        } else if (c == '>') {
            return parse_markup(reader);
        }
    } else if ((reader->mode == PARSE_PI && string_ends_with(reader->token, "?>")) ||
               (reader->mode == PARSE_COMMENT && string_ends_with(reader->token, "-->")) ||
               (reader->mode == PARSE_CDATA && string_ends_with(reader->token, "]]>"))) {
        return parse_markup(reader);
    }
    return true;
}

/* Create a reader with bounded input, nesting and structural-node counts. */
xml_reader_t *xml_reader_new(const xml_limits_t *limits, xml_event_fn callback, void *user_data)
{
    if (limits && limits->max_depth > XML_MAX_DEPTH)
        return NULL;
    xml_reader_t *reader = calloc(1, sizeof(*reader));
    if (!reader)
        return NULL;
    reader->limits.max_bytes = limits && limits->max_bytes ? limits->max_bytes : 64u * 1024u * 1024u;
    reader->limits.max_depth = limits && limits->max_depth ? limits->max_depth : XML_MAX_DEPTH;
    reader->limits.max_nodes = limits && limits->max_nodes ? limits->max_nodes : 1000000u;
    reader->callback = callback;
    reader->user_data = user_data;
    reader->first_scalar = true;
    reader->declaration_allowed = true;
    reader->token = string_new();
    reader->xml_uri = string_new_with("http://www.w3.org/XML/1998/namespace");
    if (!callback)
        reader->document = xml_new_document();
    if (!reader->token || !reader->xml_uri || (!callback && !reader->document)) {
        xml_reader_free(reader);
        return NULL;
    }
    return reader;
}

/* Strict UTF-8 decoding is the sole raw-byte boundary; lexical work receives scalars. */
bool xml_reader_feed(xml_reader_t *reader, const void *data, size_t size)
{
    if (!reader || reader->failed)
        return false;
    if (reader->finished)
        return xml_parse_fail(reader, "Cannot feed a finished XML reader");
    if (!data && size)
        return xml_parse_fail(reader, "Missing XML input bytes");
    if (size > reader->limits.max_bytes - reader->bytes)
        return xml_parse_fail(reader, "XML encoded input limit exceeded");
    reader->bytes += size;
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; ++i) {
        unsigned char byte = bytes[i];
        if (reader->utf_remaining) {
            if ((byte & 0xc0) != 0x80)
                return xml_parse_fail(reader, "Invalid UTF-8 continuation byte");
            reader->utf_value = (reader->utf_value << 6) | (byte & 0x3f);
            if (--reader->utf_remaining)
                continue;
            uint32_t value = reader->utf_value;
            if (value < reader->utf_minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                return xml_parse_fail(reader, "Invalid UTF-8 scalar or overlong encoding");
            if (!parse_scalar(reader, value))
                return false;
        } else if (byte < 0x80) {
            if (!parse_scalar(reader, byte))
                return false;
        } else if (byte >= 0xc2 && byte <= 0xdf) {
            reader->utf_value = byte & 0x1f;
            reader->utf_minimum = 0x80;
            reader->utf_remaining = 1;
        } else if (byte >= 0xe0 && byte <= 0xef) {
            reader->utf_value = byte & 0x0f;
            reader->utf_minimum = 0x800;
            reader->utf_remaining = 2;
        } else if (byte >= 0xf0 && byte <= 0xf4) {
            reader->utf_value = byte & 7;
            reader->utf_minimum = 0x10000;
            reader->utf_remaining = 3;
        } else {
            return xml_parse_fail(reader, "Invalid UTF-8 leading byte");
        }
    }
    return true;
}

/* Finish exactly once and reject incomplete encoding, markup or document structure. */
bool xml_reader_finish(xml_reader_t *reader)
{
    if (!reader || reader->failed)
        return false;
    if (reader->finished)
        return xml_parse_fail(reader, "XML reader is already finished");
    reader->finished = true;
    if (reader->utf_remaining)
        return xml_parse_fail(reader, "Truncated UTF-8 sequence");
    if (reader->mode != PARSE_TEXT)
        return xml_parse_fail(reader, "Unterminated XML markup");
    if (!parse_text_token(reader))
        return false;
    if (reader->depth || !reader->root_seen)
        return xml_parse_fail(reader, "Missing root element or unclosed XML element");
    return true;
}

/* Transfer a successfully completed tree to the caller once. */
xml_t *xml_reader_take_document(xml_reader_t *reader)
{
    if (!reader || !reader->finished || reader->failed || reader->callback)
        return NULL;
    xml_t *document = reader->document;
    reader->document = NULL;
    return document;
}

/* Borrow the first parser diagnostic, if available. */
const string_t *xml_reader_error(const xml_reader_t *reader)
{
    return reader ? reader->error : NULL;
}

/* Release parser state and any document whose ownership has not been transferred. */
void xml_reader_free(xml_reader_t *reader)
{
    if (!reader)
        return;
    while (reader->depth)
        parse_pop(reader);
    xml_free(reader->document);
    string_free(reader->token);
    string_free(reader->error);
    string_free(reader->xml_uri);
    free(reader);
}

/* Parse the encoded contents of a MARS string under the default limits. */
xml_t *xml_from_text(const string_t *text)
{
    if (!text)
        return NULL;
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    xml_t *document = NULL;
    if (reader && xml_reader_feed(reader, string_c_str(text), string_byte_length(text)) && xml_reader_finish(reader))
        document = xml_reader_take_document(reader);
    xml_reader_free(reader);
    return document;
}

/* Read bounded byte blocks through the file API and parse incrementally. */
xml_t *xml_from_file(const string_t *path)
{
    if (!path)
        return NULL;
    file_t *file = file_new(path);
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    xml_t *document = NULL;
    bool ok = file && reader && file_open_read(file);
    unsigned char bytes[8192];
    while (ok) {
        size_t count = 0;
        if (!file_read(file, bytes, sizeof(bytes), &count)) {
            ok = false;
            break;
        }
        if (!count)
            break;
        ok = xml_reader_feed(reader, bytes, count);
    }
    if (ok)
        ok = xml_reader_finish(reader);
    if (file && file_is_open(file) && !file_close(file))
        ok = false;
    if (ok)
        document = xml_reader_take_document(reader);
    xml_reader_free(reader);
    file_free(file);
    return document;
}
