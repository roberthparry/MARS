#ifndef MARS_XML_PARSE_PRIVATE_H
#define MARS_XML_PARSE_PRIVATE_H
#if !defined(MARS_XML_INTERNAL_ACCESS)
#error "xml_parse_private.h is private to the XML parser."
#endif
#include <stdint.h>
#include "xml_internal.h"

typedef enum { PARSE_TEXT, PARSE_OPEN, PARSE_TAG, PARSE_PI, PARSE_COMMENT, PARSE_CDATA } parse_mode_t;

typedef struct {
    string_t *name;
    dictionary_t *namespaces;
    xml_t *node; /* Borrowed from the document; absent in event-only mode. */
} parse_frame_t;

typedef struct {
    string_cursor_t *cursor;
    string_view_t view;
} parse_cursor_t;

struct _xml_reader_t {
    xml_limits_t limits;
    xml_event_fn callback;
    void *user_data;
    xml_t *document;
    string_t *token;
    string_t *error;
    string_t *xml_uri;
    parse_frame_t frames[XML_MAX_DEPTH];
    size_t depth, bytes, nodes;
    uint32_t utf_value, utf_minimum;
    unsigned utf_remaining;
    unsigned brackets;
    uint32_t quote;
    parse_mode_t mode;
    bool failed, finished, root_seen, declaration_allowed, first_scalar, previous_cr, reference;
};

/* Shared scalar cursor operations and diagnostics for parser implementation files. */
bool xml_parse_fail(xml_reader_t *reader, const char *message);
bool xml_parse_name_start(uint32_t c);
bool xml_parse_name_char(uint32_t c);
uint32_t xml_parse_peek(const parse_cursor_t *input);
void xml_parse_next(parse_cursor_t *input);
bool xml_parse_literal(const string_t *text, const char *literal);
const string_t *xml_parse_lookup(const dictionary_t *dict, const string_t *key);
bool xml_parse_namespaces(xml_reader_t *reader, const dictionary_t *attributes);

#endif
