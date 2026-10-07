/* http_content.c - JSON/XML adapters, leaving transport and interpretation separate. */
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static bool set_document(http_request_t *request, string_t *text, const char *mime)
{
    string_t *type = string_new_with(mime);
    bool ok = text && type && http_request_set_text(request, text, type);
    string_free(type);
    string_free(text);
    return ok;
}

/* Serialise using the JSON module, retaining the caller's tree. */
bool http_request_set_json(http_request_t *request, const json_t *json)
{
    return json && set_document(request, json_to_string(json), "application/json");
}

/* Serialise using the native XML module, retaining the caller's tree. */
bool http_request_set_xml(http_request_t *request, const xml_t *xml)
{
    return xml && set_document(request, xml_to_string(xml), "application/xml");
}

/* Decode buffered UTF-8 strictly without changing Unicode spelling. */
string_t *http_response_text(const http_response_t *response)
{
    if (!response || response->streamed)
        return NULL;
    string_t *text = string_new();
    if (!text)
        return NULL;
    if (string_append_utf8_exact(text, http_response_body(response), http_response_body_size(response)) != 0) {
        string_free(text);
        return NULL;
    }
    return text;
}

/* Parse a buffered UTF-8 response as JSON without assuming HTTP success. */
json_t *http_response_json(const http_response_t *response)
{
    string_t *text = http_response_text(response);
    json_t *json = text ? json_from_text(text) : NULL;
    string_free(text);
    return json;
}

/* Parse a buffered UTF-8 response as XML without assuming HTTP success. */
xml_t *http_response_xml(const http_response_t *response)
{
    string_t *text = http_response_text(response);
    xml_t *xml = text ? xml_from_text(text) : NULL;
    string_free(text);
    return xml;
}
