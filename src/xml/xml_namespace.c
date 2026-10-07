/* xml_namespace.c - scoped XML namespace and expanded-attribute validation. */
#define MARS_XML_INTERNAL_ACCESS
#include "xml_parse_private.h"

/* Internal dictionaries share the typed string storage, but keys need not be XML names. */
static bool parse_store(xml_reader_t *reader, dictionary_t *dict, const string_t *key, const string_t *value)
{
    string_t *key_copy = string_clone(key), *value_copy = string_clone(value);
    if (key_copy && value_copy && dictionary_set(dict, &key_copy, &value_copy))
        return true;
    string_free(key_copy);
    string_free(value_copy);
    return xml_parse_fail(reader, "Cannot allocate namespace table");
}

static bool parse_qname(xml_reader_t *reader, const string_t *name, string_t **prefix, string_t **local)
{
    parse_cursor_t input = {string_cursor_new(name), string_view_all(name)};
    *prefix = NULL;
    *local = NULL;
    if (!input.cursor)
        return xml_parse_fail(reader, "Cannot allocate qualified-name parser");
    string_pos_t start = string_cursor_position(input.cursor);
    bool first = true, colon = false;
    while (!string_cursor_done(input.cursor)) {
        uint32_t c = xml_parse_peek(&input);
        if (c == ':') {
            if (first || colon)
                break;
            *prefix = string_cursor_extract(start, input.cursor);
            if (!*prefix)
                break;
            colon = true;
            first = true;
            xml_parse_next(&input);
            start = string_cursor_position(input.cursor);
            continue;
        }
        if (first ? !xml_parse_name_start(c) : !xml_parse_name_char(c))
            break;
        first = false;
        xml_parse_next(&input);
    }
    bool ok = !first && string_cursor_done(input.cursor);
    if (ok)
        *local = string_cursor_extract(start, input.cursor);
    string_cursor_free(input.cursor);
    if (ok && *local)
        return true;
    string_free(*prefix);
    *prefix = NULL;
    return xml_parse_fail(reader, "Invalid namespace-qualified XML name");
}

static const string_t *parse_namespace(xml_reader_t *reader, const string_t *prefix)
{
    if (prefix && xml_parse_literal(prefix, "xml"))
        return reader->xml_uri;
    string_t *key = string_new_with(prefix ? "xmlns:" : "xmlns");
    if (key && prefix && string_append_utf8_exact(key, string_c_str(prefix), string_byte_length(prefix)) != 0) {
        string_free(key);
        key = NULL;
    }
    if (!key) {
        xml_parse_fail(reader, "Cannot allocate namespace lookup");
        return NULL;
    }
    const string_t *uri = NULL;
    /* The only scope walk is bounded by the hard nesting limit of 128. */
    for (size_t depth = reader->depth; depth && !uri; --depth)
        uri = xml_parse_lookup(reader->frames[depth - 1].namespaces, key);
    string_free(key);
    return uri;
}

bool xml_parse_namespaces(xml_reader_t *reader, const dictionary_t *attributes)
{
    parse_frame_t *frame = &reader->frames[reader->depth - 1];
    const char *xmlns_uri = "http://www.w3.org/2000/xmlns/";
    for (size_t i = 0; i < dictionary_size(attributes); ++i) {
        const string_t *key = *(string_t *const *)dictionary_get_key(attributes, i);
        const string_t *value = *(string_t *const *)dictionary_get_value(attributes, i);
        string_t *prefix, *local;
        if (!parse_qname(reader, key, &prefix, &local))
            return false;
        bool declaration = (!prefix && xml_parse_literal(local, "xmlns")) || (prefix && xml_parse_literal(prefix, "xmlns"));
        if (declaration) {
            bool xml = prefix && xml_parse_literal(local, "xml");
            bool invalid = (prefix && xml_parse_literal(local, "xmlns")) || xml_parse_literal(value, xmlns_uri) ||
                           (xml != (string_compare(value, reader->xml_uri) == 0)) ||
                           (prefix && !string_byte_length(value));
            if (invalid)
                xml_parse_fail(reader, "Invalid reserved namespace declaration or empty prefix binding");
            else
                parse_store(reader, frame->namespaces, key, value);
        }
        string_free(prefix);
        string_free(local);
        if (reader->failed)
            return false;
    }
    string_t *prefix, *local;
    if (!parse_qname(reader, frame->name, &prefix, &local))
        return false;
    if (prefix && (xml_parse_literal(prefix, "xmlns") || !parse_namespace(reader, prefix)))
        xml_parse_fail(reader, "Undeclared or reserved element namespace prefix");
    string_free(prefix);
    string_free(local);
    dictionary_t *expanded = xml_attribute_storage_new();
    if (!expanded)
        xml_parse_fail(reader, "Cannot allocate expanded attribute table");
    for (size_t i = 0; !reader->failed && i < dictionary_size(attributes); ++i) {
        const string_t *key = *(string_t *const *)dictionary_get_key(attributes, i);
        if (!parse_qname(reader, key, &prefix, &local))
            break;
        bool declaration = (!prefix && xml_parse_literal(local, "xmlns")) || (prefix && xml_parse_literal(prefix, "xmlns"));
        if (!declaration) {
            const string_t *uri = prefix ? parse_namespace(reader, prefix) : NULL;
            if (prefix && !uri) {
                xml_parse_fail(reader, "Undeclared attribute namespace prefix");
            } else {
                /* U+001F cannot occur in an XML namespace URI or local name. */
                string_t *identity = uri ? string_clone(uri) : string_new();
                if (identity && (string_append_utf8_exact(identity, "\x1f", 1) != 0 ||
                    string_append_utf8_exact(identity, string_c_str(local), string_byte_length(local)) != 0)) {
                    string_free(identity);
                    identity = NULL;
                }
                if (!identity)
                    xml_parse_fail(reader, "Cannot allocate expanded attribute name");
                else if (xml_parse_lookup(expanded, identity))
                    xml_parse_fail(reader, "Duplicate expanded attribute name");
                else
                    parse_store(reader, expanded, identity, local);
                string_free(identity);
            }
        }
        string_free(prefix);
        string_free(local);
    }
    xml_attribute_storage_free(expanded);
    return !reader->failed;
}
