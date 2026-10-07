/**
 * @file http_get.c
 * @brief Runnable HTTP GET and JSON-response example.
 *
 * Requests an echoed query value and checks the parsed response before printing it. An optional URL allows the
 * same main program to run against authorised local test fixtures.
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
    string_t *url = string_new_with(argc == 2 ? argv[1] : "https://httpbin.org/get?message=MARS");
    string_t *args_key = string_new_with("args"), *message_key = string_new_with("message");
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_response_t *response = http_client_send(client, request);
    json_t *json = http_response_ok(response) ? http_response_json(response) : NULL;
    const json_t *args = json_object_get(json, args_key);
    const string_t *message = json_string_value(json_object_get(args, message_key));
    bool valid = http_response_status(response) == 200 && message &&
                 string_view_equals_literal(string_view_all(message), "MARS");
    if (valid)
        string_printf("status=%ld\nmessage=%S\n", http_response_status(response), message);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    json_free(json); string_free(args_key); string_free(message_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
