/**
 * @file http_post_xml.c
 * @brief Runnable XML POST example.
 *
 * Constructs an XML document and verifies a parsed echo response from a compatible endpoint. It demonstrates
 * document ownership and transport checks in a complete C main program.
 */

#include <stdlib.h>

#include "http.h"

/* Run the documented example; an optional URL allows deterministic offline tests. */
int main(int argc, char **argv)
{
    if (argc > 2) {
        string_printf("Usage: %s [URL]\n", argv[0]);
        return EXIT_FAILURE;
    }
    string_t *url = string_new_with(argc == 2 ? argv[1] : "https://httpbin.org/post");
    string_t *text = string_new_with("<answer>42</answer>");
    string_t *data_key = string_new_with("data");
    xml_t *document = xml_from_text(text);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ready = http_request_set_xml(request, document);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    /* httpbin wraps the echoed XML body in the JSON field "data". */
    json_t *envelope = http_response_ok(response) ? http_response_json(response) : NULL;
    const string_t *echo = json_string_value(json_object_get(envelope, data_key));
    xml_t *reply = echo ? xml_from_text(echo) : NULL;
    string_t *output = reply ? xml_to_string(reply) : NULL;
    bool valid = http_response_status(response) == 200 && output &&
                 string_view_equals_literal(string_view_all(output), "<answer>42</answer>");
    if (valid)
        string_printf("status=%ld\n%S\n", http_response_status(response), output);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    string_free(output); xml_free(reply); xml_free(document); json_free(envelope);
    string_free(text); string_free(data_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
