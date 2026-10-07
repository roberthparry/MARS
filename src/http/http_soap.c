/* SOAP envelope construction and atomic request preparation. */
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static const char *const namespaces[] = {[HTTP_SOAP_11] = "http://schemas.xmlsoap.org/soap/envelope/",
                                         [HTTP_SOAP_12] = "http://www.w3.org/2003/05/soap-envelope"};

static xml_t *element(const char *name)
{
    string_t *text = string_new_with(name);
    xml_t *node = text ? xml_new_element(text) : NULL;
    string_free(text);
    return node;
}

/* Deep-copy payloads into a namespace-qualified SOAP envelope. */
xml_t *http_soap_envelope(http_soap_version_t version, const xml_t *payload, const xml_t *header)
{
    if ((version != HTTP_SOAP_11 && version != HTTP_SOAP_12) || xml_type(payload) != XML_ELEMENT ||
        (header && xml_type(header) != XML_ELEMENT))
        return NULL;
    xml_t *envelope = element("soap:Envelope"), *body = element("soap:Body");
    xml_t *head = header ? element("soap:Header") : NULL;
    string_t *key = string_new_with("xmlns:soap"), *uri = string_new_with(namespaces[version]);
    bool ok = envelope && body && key && uri && xml_set_attribute(envelope, key, uri) &&
              (!header || (head && xml_append_child(head, header) && xml_append_child(envelope, head))) &&
              xml_append_child(body, payload) && xml_append_child(envelope, body);
    string_free(key);
    string_free(uri);
    xml_free(body);
    xml_free(head);
    if (!ok) {
        xml_free(envelope);
        return NULL;
    }
    /* Reject undeclared prefixes in the supplied fragments before publishing the tree. */
    string_t *check = xml_to_string(envelope);
    xml_t *validated = check ? xml_from_text(check) : NULL;
    string_free(check);
    if (!validated) {
        xml_free(envelope);
        return NULL;
    }
    xml_free(validated);
    return envelope;
}

static dictionary_t *copy_headers(const dictionary_t *source)
{
    dictionary_t *copy = http_headers_new();
    bool ok = copy != NULL;
    for (size_t entry = 0; ok && entry < dictionary_size(source); ++entry) {
        const string_t *key = *(string_t *const *)dictionary_get_key(source, entry);
        const array_t *values = *(array_t *const *)dictionary_get_value(source, entry);
        if (string_view_equals_literal(string_view_all(key), "soapaction"))
            continue;
        for (size_t i = 0; ok && i < array_size(values); ++i)
            ok = http_headers_store(copy, key, *(string_t *const *)array_get(values, i), false);
    }
    if (!ok) {
        http_headers_free(copy);
        return NULL;
    }
    return copy;
}

/* Prepare headers and body independently before replacing the caller's request state. */
bool http_request_set_soap(http_request_t *request, http_soap_version_t version, const string_t *action,
                           const xml_t *payload, const xml_t *header)
{
    if (!request || request->method != HTTP_POST ||
        (action &&
         (!http_ascii_text(action, true) || string_find(action, "\"") >= 0 || string_find(action, "\\") >= 0)))
        return false;
    xml_t *envelope = http_soap_envelope(version, payload, header);
    string_t *text = envelope ? xml_to_string(envelope) : NULL;
    string_t *empty = action ? NULL : string_new();
    const string_t *effective = action ? action : empty;
    string_t *type = effective ? (version == HTTP_SOAP_11
                                      ? string_new_with("text/xml; charset=utf-8")
                                      : string_sprintf("application/soap+xml; charset=utf-8; action=\"%S\"", effective))
                               : NULL;
    string_t *quoted = effective ? string_sprintf("\"%S\"", effective) : NULL;
    string_t *name = string_new_with("SOAPAction");
    http_request_t temporary = {.method = HTTP_POST, .headers = copy_headers(request->headers)};
    bool ok = text && type && quoted && name && temporary.headers && http_request_set_text(&temporary, text, type) &&
              (version != HTTP_SOAP_11 || http_request_set_header(&temporary, name, quoted));
    if (ok) {
        http_headers_free(request->headers);
        free(request->body);
        string_free(request->body_path);
        request->headers = temporary.headers;
        request->body = temporary.body;
        request->body_size = temporary.body_size;
        request->body_path = NULL;
        request->has_body = true;
    } else {
        http_headers_free(temporary.headers);
        free(temporary.body);
    }
    xml_free(envelope);
    string_free(text);
    string_free(type);
    string_free(empty);
    string_free(quoted);
    string_free(name);
    return ok;
}
