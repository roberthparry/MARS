#ifndef MARS_XML_INTERNAL_H
#define MARS_XML_INTERNAL_H
#if !defined(MARS_XML_INTERNAL_ACCESS)
#error "xml_internal.h is private to the XML module."
#endif
#include "xml.h"
#include "array.h"
#define XML_MAX_DEPTH 128u
struct _xml_t {
    xml_type_t type;
    string_t *name;
    string_t *text;
    dictionary_t *attributes;
    array_t *children;
};
bool xml_attach_owned(xml_t *parent, xml_t *child);
bool xml_valid_name(const string_t *name);
bool xml_valid_text(const string_t *text);
dictionary_t *xml_attribute_storage_new(void);
void xml_attribute_storage_free(dictionary_t *dict);
bool xml_attribute_store(dictionary_t *dict, const string_t *name, const string_t *value);
#endif
