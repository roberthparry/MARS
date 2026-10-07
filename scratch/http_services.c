/**
 * @file http_services.c
 * @brief Runnable form, SOAP and event-reader examples.
 *
 * Exercises request-body encoding, SOAP envelope handling and incremental event decoding. It demonstrates protocol
 * helpers without requiring every operation to contact a remote service.
 */

#include "http.h"
#include <stdio.h>
#include <stdlib.h>

static bool show_event(const string_t *event, const string_t *data, const string_t *id, bool has_retry,
                       uint64_t retry_ms, void *context)
{
    (void)id;
    (void)has_retry;
    (void)retry_ms;
    unsigned *count = context;
    ++*count;
    string_printf("%S: %S\n", event, data);
    return true;
}

int main(void)
{
    http_form_t *form = http_form_new();
    string_t *name = string_new_with("message"), *value = string_new_with("hello & goodbye");
    bool ok = form && name && value && http_form_add_text(form, name, value);
    string_t *encoded = ok ? http_form_encode(form) : NULL;
    if (encoded)
        string_printf("%S\n", encoded);
    else
        ok = false;

    string_t *text = string_new_with("<answer>42</answer>");
    xml_t *document = text ? xml_from_text(text) : NULL;
    xml_t *envelope = document ? http_soap_envelope(HTTP_SOAP_12, xml_child_at(document, 0), NULL) : NULL;
    http_soap_t *soap = envelope ? http_soap_parse(envelope) : NULL;
    if (soap)
        printf("SOAP body children: %zu\n", xml_child_count(http_soap_body(soap)));
    else
        ok = false;

    unsigned events = 0;
    http_event_reader_t *reader = http_event_reader_new(1024, show_event, &events);
    const char event[] = "event: answer\ndata: 42\n\n";
    ok = reader && http_event_reader_feed(reader, event, sizeof(event) - 1) && http_event_reader_finish(reader) &&
         events == 1 && ok;
    http_event_reader_free(reader);
    http_soap_free(soap);
    xml_free(envelope);
    xml_free(document);
    string_free(text);
    string_free(encoded);
    string_free(name);
    string_free(value);
    http_form_free(form);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
