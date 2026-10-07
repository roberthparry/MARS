/* xml_core.c - owned XML nodes and dictionary-backed attributes. */
#include <stdint.h>
#include <stdlib.h>
#define MARS_XML_INTERNAL_ACCESS
#include "xml_internal.h"

static bool name_start(uint32_t c)
{
    return c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) || (c >= 0xf8 && c <= 0x2ff) ||
           (c >= 0x370 && c <= 0x37d) || (c >= 0x37f && c <= 0x1fff) || (c >= 0x200c && c <= 0x200d) ||
           (c >= 0x2070 && c <= 0x218f) || (c >= 0x2c00 && c <= 0x2fef) || (c >= 0x3001 && c <= 0xd7ff) ||
           (c >= 0xf900 && c <= 0xfdcf) || (c >= 0xfdf0 && c <= 0xfffd) || (c >= 0x10000 && c <= 0xeffff);
}

bool xml_valid_text(const string_t *text)
{
    string_t *validated;
    if (!text || !(validated = string_new()))
        return false;
    bool ok = string_append_utf8_exact(validated, string_c_str(text), string_byte_length(text)) == 0;
    string_free(validated);
    string_view_t view = string_view_all(text);
    for (string_pos_t pos = 0; ok && pos < string_view_length(view);) {
        uint32_t c;
        string_pos_t next;
        ok = string_view_peek_rune_value(view, pos, &c, &next);
        if (!ok)
            break;
        ok = c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0xd7ff) ||
             (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
        pos = next;
    }
    return ok;
}

bool xml_valid_name(const string_t *name)
{
    if (!xml_valid_text(name) || !string_byte_length(name))
        return false;
    string_view_t view = string_view_all(name);
    bool first = true;
    for (string_pos_t pos = 0; pos < string_view_length(view);) {
        uint32_t c;
        string_pos_t next;
        if (!string_view_peek_rune_value(view, pos, &c, &next))
            return false;
        if (!name_start(c) && (first || !(c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xb7 ||
                                        (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040))))
            return false;
        first = false;
        pos = next;
    }
    return true;
}

static size_t key_hash(const void *key) { return (size_t)string_hash(*(string_t *const *)key); }
static int key_compare(const void *a, const void *b)
{
    return string_compare(*(string_t *const *)a, *(string_t *const *)b);
}
static void node_destroy(void *value) { xml_free(*(xml_t **)value); }

dictionary_t *xml_attribute_storage_new(void)
{
    return dictionary_create(sizeof(string_t *), sizeof(string_t *), key_hash, key_compare, NULL,
                             NULL, NULL, NULL, NULL);
}

/* Keep ownership outside dictionary_set: its rollback may destroy stored shallow copies on failure. */
void xml_attribute_storage_free(dictionary_t *dict)
{
    for (size_t i = 0; i < dictionary_size(dict); ++i) {
        string_free(*(string_t *const *)dictionary_get_key(dict, i));
        string_free(*(string_t *const *)dictionary_get_value(dict, i));
    }
    dictionary_destroy(dict);
}

bool xml_attribute_store(dictionary_t *dict, const string_t *name, const string_t *value)
{
    dictionary_entry_t *entry = NULL;
    string_t *key = NULL, *copy;
    if (!dict || !xml_valid_name(name) || !xml_valid_text(value))
        return false;
    copy = string_clone(value);
    if (!copy)
        return false;
    if (dictionary_get_entry(dict, &name, &entry)) {
        string_t *old = *(string_t *const *)dictionary_entry_value(entry);
        if (dictionary_set_entry(dict, entry, &copy)) {
            string_free(old);
            return true;
        }
    } else {
        key = string_clone(name);
        if (key && dictionary_set(dict, &key, &copy))
            return true;
    }
    string_free(key);
    string_free(copy);
    return false;
}

static xml_t *node_new(xml_type_t type, const string_t *name, const string_t *text)
{
    xml_t *node = calloc(1, sizeof(*node));
    if (!node)
        return NULL;
    node->type = type;
    if (name && !(node->name = string_clone(name)))
        goto fail;
    if (text && !(node->text = string_clone(text)))
        goto fail;
    if (type == XML_ELEMENT && !(node->attributes = xml_attribute_storage_new()))
        goto fail;
    if ((type == XML_DOCUMENT || type == XML_ELEMENT) &&
        !(node->children = array_create(sizeof(xml_t *), NULL, node_destroy)))
        goto fail;
    return node;
fail:
    xml_free(node);
    return NULL;
}

/* Create an empty document container. */
xml_t *xml_new_document(void) { return node_new(XML_DOCUMENT, NULL, NULL); }

/* Copy an XML element name into an owned node. */
xml_t *xml_new_element(const string_t *name)
{
    return xml_valid_name(name) ? node_new(XML_ELEMENT, name, NULL) : NULL;
}

/* Copy XML character data into an owned text node. */
xml_t *xml_new_text(const string_t *text)
{
    return xml_valid_text(text) ? node_new(XML_TEXT, NULL, text) : NULL;
}

/* Validate and copy comment content. */
xml_t *xml_new_comment(const string_t *text)
{
    size_t length = text ? string_byte_length(text) : 0;
    unsigned char last = 0;
    if (length)
        string_view_peek_ascii(string_view_all(text), length - 1, &last);
    if (!xml_valid_text(text) || string_find(text, "--") >= 0 ||
        last == '-')
        return NULL;
    return node_new(XML_COMMENT, NULL, text);
}

/* Validate and copy a processing instruction. */
xml_t *xml_new_processing_instruction(const string_t *name, const string_t *text)
{
    if (!xml_valid_name(name) || !xml_valid_text(text) || string_find(text, "?>") >= 0)
        return NULL;
    if (string_byte_length(name) == 3) {
        uint32_t a = rune_value(string_at(name, 0)), b = rune_value(string_at(name, 1));
        uint32_t c = rune_value(string_at(name, 2));
        if ((a == 'x' || a == 'X') && (b == 'm' || b == 'M') && (c == 'l' || c == 'L'))
            return NULL;
    }
    return node_new(XML_PROCESSING_INSTRUCTION, name, text);
}

/* Destroy a node and all recursively owned storage. */
void xml_free(xml_t *xml)
{
    if (!xml)
        return;
    string_free(xml->name);
    string_free(xml->text);
    xml_attribute_storage_free(xml->attributes);
    array_destroy(xml->children);
    free(xml);
}

/* Read a node category. */
xml_type_t xml_type(const xml_t *xml) { return xml ? xml->type : XML_INVALID; }
/* Borrow a stored name. */
const string_t *xml_name(const xml_t *xml) { return xml ? xml->name : NULL; }
/* Borrow stored character data. */
const string_t *xml_text(const xml_t *xml) { return xml ? xml->text : NULL; }
/* Expose attributes through the dictionary API without transferring ownership. */
const dictionary_t *xml_attributes(const xml_t *xml) { return xml ? xml->attributes : NULL; }
/* Store copied attribute text. */
bool xml_set_attribute(xml_t *xml, const string_t *name, const string_t *value)
{
    return xml && xml->type == XML_ELEMENT && xml_attribute_store(xml->attributes, name, value);
}
/* Borrow a dictionary value by attribute name. */
const string_t *xml_get_attribute(const xml_t *xml, const string_t *name)
{
    dictionary_entry_t *entry = NULL;
    if (!xml || !name || !xml->attributes || !dictionary_get_entry(xml->attributes, &name, &entry))
        return NULL;
    return *(string_t *const *)dictionary_entry_value(entry);
}
/* Count ordered content nodes. */
size_t xml_child_count(const xml_t *xml) { return xml && xml->children ? array_size(xml->children) : 0; }
/* Borrow one ordered child. */
const xml_t *xml_child_at(const xml_t *xml, size_t index)
{
    xml_t *const *child = xml && xml->children ? array_get(xml->children, index) : NULL;
    return child ? *child : NULL;
}

bool xml_attach_owned(xml_t *parent, xml_t *child)
{
    if (!parent || !child || !parent->children || child->type == XML_DOCUMENT)
        return false;
    if (parent->type == XML_DOCUMENT) {
        if (child->type == XML_TEXT)
            return false;
        if (child->type == XML_ELEMENT) {
            for (size_t i = 0; i < xml_child_count(parent); ++i)
                if (xml_child_at(parent, i)->type == XML_ELEMENT)
                    return false;
        }
    }
    return array_append_carray(parent->children, &child, 1);
}

static size_t tree_depth(const xml_t *xml)
{
    size_t depth = 0;
    for (size_t i = 0; i < xml_child_count(xml); ++i) {
        size_t child_depth = tree_depth(xml_child_at(xml, i));
        if (child_depth > depth)
            depth = child_depth;
    }
    return depth + (xml->type == XML_ELEMENT);
}

/* Deep-copy a node and its attribute dictionary and ordered content. */
xml_t *xml_clone(const xml_t *xml)
{
    xml_t *copy;
    if (!xml || !(copy = node_new(xml->type, xml->name, xml->text)))
        return NULL;
    for (size_t i = 0; i < dictionary_size(xml->attributes); ++i) {
        string_t *const *key = dictionary_get_key(xml->attributes, i);
        string_t *const *value = dictionary_get_value(xml->attributes, i);
        if (!xml_attribute_store(copy->attributes, *key, *value))
            goto fail;
    }
    for (size_t i = 0; i < xml_child_count(xml); ++i) {
        xml_t *child = xml_clone(xml_child_at(xml, i));
        if (!child || !xml_attach_owned(copy, child)) {
            xml_free(child);
            goto fail;
        }
    }
    return copy;
fail:
    xml_free(copy);
    return NULL;
}

/* Append an independent copy, retaining the caller's ownership. */
bool xml_append_child(xml_t *xml, const xml_t *child)
{
    xml_t *copy;
    if (!xml || !child || !xml->children ||
        tree_depth(child) + (xml->type == XML_ELEMENT) > XML_MAX_DEPTH)
        return false;
    copy = xml_clone(child);
    if (!copy || !xml_attach_owned(xml, copy)) {
        xml_free(copy);
        return false;
    }
    return true;
}
