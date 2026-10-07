/* Namespace-aware SOAP envelope and fault inspection. */
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

struct _http_soap_t {
    xml_t *document;
    const xml_t *body;
    bool fault;
    string_t *code, *reason;
};

static bool equal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

/* SOAP structure is at most five levels deep; bounded ancestor lookup resolves namespace aliases. */
static bool named(const xml_t *const *path, size_t depth, const char *local, const char *uri)
{
    const xml_t *node = path[depth - 1];
    if (xml_type(node) != XML_ELEMENT)
        return false;
    const string_t *name = xml_name(node);
    string_offset_t colon = string_find(name, ":");
    string_t *leaf =
        colon < 0 ? string_clone(name) : string_substr(name, colon + 1, string_byte_length(name) - colon - 1);
    string_t *prefix = colon < 0 ? NULL : string_substr(name, 0, colon);
    string_t *key = colon < 0 ? string_new_with("xmlns") : (prefix ? string_sprintf("xmlns:%S", prefix) : NULL);
    const string_t *namespace = NULL;
    for (size_t i = depth; key && i && !namespace; --i)
        namespace = xml_get_attribute(path[i - 1], key);
    bool ok = key && equal(leaf, local) && (uri ? equal(namespace, uri) : !namespace || equal(namespace, ""));
    string_free(leaf);
    string_free(prefix);
    string_free(key);
    return ok;
}

static bool ignorable(const xml_t *node)
{
    if (xml_type(node) == XML_COMMENT || xml_type(node) == XML_PROCESSING_INSTRUCTION)
        return true;
    if (xml_type(node) != XML_TEXT)
        return false;
    string_cursor_t *cursor = string_cursor_new(xml_text(node));
    if (!cursor)
        return false;
    bool ok = true;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch;
        ok = string_cursor_peek_ascii(cursor, &ch) && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return ok;
}

static const xml_t *only_element(const xml_t *parent)
{
    const xml_t *result = NULL;
    for (size_t i = 0; i < xml_child_count(parent); ++i) {
        const xml_t *child = xml_child_at(parent, i);
        if (xml_type(child) == XML_ELEMENT) {
            if (result)
                return NULL;
            result = child;
        } else if (!ignorable(child))
            return NULL;
    }
    return result;
}

static string_t *text_content(const xml_t *node)
{
    if (!node)
        return NULL;
    string_t *text = string_new();
    bool ok = text != NULL;
    for (size_t i = 0; ok && i < xml_child_count(node); ++i) {
        const xml_t *child = xml_child_at(node, i);
        if (xml_type(child) == XML_TEXT) {
            const string_t *part = xml_text(child);
            ok = !string_append_utf8_exact(text, string_c_str(part), string_byte_length(part));
        } else
            ok = xml_type(child) == XML_COMMENT;
    }
    if (!ok || !string_byte_length(text)) {
        string_free(text);
        return NULL;
    }
    return text;
}

static const xml_t *find_child(const xml_t **path, size_t depth, const char *local, const char *uri)
{
    const xml_t *parent = path[depth - 1], *found = NULL;
    for (size_t i = 0; i < xml_child_count(parent); ++i) {
        path[depth] = xml_child_at(parent, i);
        if (named(path, depth + 1, local, uri)) {
            if (found)
                return NULL;
            found = path[depth];
        }
    }
    path[depth] = found;
    return found;
}

/* Validate namespace-qualified structure before exposing any payload or fault. */
http_soap_t *http_soap_parse(const xml_t *document)
{
    if (!document)
        return NULL;
    /* Serialisation and parsing also validate programmatically constructed namespace bindings. */
    string_t *source = xml_to_string(document);
    http_soap_t *soap = calloc(1, sizeof(*soap));
    if (soap)
        soap->document = source ? xml_from_text(source) : NULL;
    string_free(source);
    if (!soap || !soap->document)
        goto fail;
    const xml_t *path[6] = {only_element(soap->document)};
    if (!path[0])
        goto fail;
    const char *uri = "http://schemas.xmlsoap.org/soap/envelope/";
    bool version12 = false;
    if (!named(path, 1, "Envelope", uri)) {
        uri = "http://www.w3.org/2003/05/soap-envelope";
        version12 = true;
        if (!named(path, 1, "Envelope", uri))
            goto fail;
    }
    bool header_seen = false;
    for (size_t i = 0; i < xml_child_count(path[0]); ++i) {
        path[1] = xml_child_at(path[0], i);
        if (named(path, 2, "Header", uri)) {
            if (header_seen || soap->body)
                goto fail;
            header_seen = true;
            for (size_t j = 0; j < xml_child_count(path[1]); ++j)
                if (!ignorable(xml_child_at(path[1], j)))
                    goto fail;
        } else if (named(path, 2, "Body", uri)) {
            if (soap->body)
                goto fail;
            soap->body = path[1];
        } else if (!ignorable(path[1]))
            goto fail;
    }
    if (!soap->body)
        goto fail;
    path[1] = soap->body;
    const xml_t *fault = find_child(path, 2, "Fault", uri);
    /* Duplicate Fault elements must not become a successful non-fault response. */
    for (size_t i = 0; i < xml_child_count(soap->body); ++i) {
        path[2] = xml_child_at(soap->body, i);
        if (named(path, 3, "Fault", uri) && (!fault || only_element(soap->body) != fault))
            goto fail;
        if (xml_type(path[2]) != XML_ELEMENT && !ignorable(path[2]))
            goto fail;
    }
    if (!fault)
        return soap;
    soap->fault = true;
    path[2] = fault;
    if (!version12) {
        soap->code = text_content(find_child(path, 3, "faultcode", NULL));
        soap->reason = text_content(find_child(path, 3, "faultstring", NULL));
    } else {
        const xml_t *code = find_child(path, 3, "Code", uri);
        if (code)
            soap->code = text_content(find_child(path, 4, "Value", uri));
        path[2] = fault;
        const xml_t *reason = find_child(path, 3, "Reason", uri);
        if (reason) {
            for (size_t i = 0; i < xml_child_count(reason); ++i) {
                path[4] = xml_child_at(reason, i);
                if (named(path, 5, "Text", uri)) {
                    soap->reason = text_content(path[4]);
                    break;
                }
            }
        }
    }
    if (soap->code && soap->reason)
        return soap;
fail:
    http_soap_free(soap);
    return NULL;
}

/* Release a validated envelope and all owned fault strings. */
void http_soap_free(http_soap_t *soap)
{
    if (soap) {
        xml_free(soap->document);
        string_free(soap->code);
        string_free(soap->reason);
        free(soap);
    }
}

/* Borrow the validated Body. */
const xml_t *http_soap_body(const http_soap_t *soap)
{
    return soap ? soap->body : NULL;
}

/* Distinguish protocol faults from normal payloads. */
bool http_soap_is_fault(const http_soap_t *soap)
{
    return soap && soap->fault;
}

/* Borrow extracted fault code text. */
const string_t *http_soap_fault_code(const http_soap_t *soap)
{
    return soap ? soap->code : NULL;
}

/* Borrow the first extracted fault reason. */
const string_t *http_soap_fault_reason(const http_soap_t *soap)
{
    return soap ? soap->reason : NULL;
}
