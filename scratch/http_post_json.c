/**
 * @file http_post_json.c
 * @brief Runnable JSON POST example.
 *
 * Builds a JSON request, sends it and validates the echoed answer before cleanup. The optional endpoint argument
 * lets the documentation test exercise the same executable logic locally.
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
    string_t *text = string_new_with("{\"answer\":42}");
    string_t *json_key = string_new_with("json"), *answer_key = string_new_with("answer");
    json_t *payload = json_from_text(text);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ready = http_request_set_json(request, payload);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    json_t *reply = http_response_ok(response) ? http_response_json(response) : NULL;
    const json_t *echo = json_object_get(reply, json_key);
    const string_t *answer = json_number_text(json_object_get(echo, answer_key));
    bool valid = http_response_status(response) == 200 && answer &&
                 string_view_equals_literal(string_view_all(answer), "42");
    if (valid)
        string_printf("status=%ld\nanswer=%S\n", http_response_status(response), answer);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    json_free(reply); json_free(payload);
    string_free(text); string_free(json_key); string_free(answer_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
