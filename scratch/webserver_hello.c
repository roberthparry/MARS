/**
 * @file webserver_hello.c
 * @brief Runnable local web-service example.
 *
 * Starts a local web server, registers a JSON greeting route and verifies it through the HTTP client. The program
 * owns child-process and server cleanup so the example can run deterministically in tests.
 */

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "webserver.h"
#include "http.h"

static bool hello(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)request;
    (void)context;
    string_t *text = string_new_with("{\"message\":\"Hello from MARS\"}");
    json_t *json = text ? json_from_text(text) : NULL;
    bool ok = json && websrv_response_json(response, json);
    json_free(json);
    string_free(text);
    return ok;
}

int main(void)
{
    websrv_t *server = websrv_new(NULL, 0, NULL);
    string_t *path = string_new_with("/hello");
    bool ready = server && path && websrv_route(server, HTTP_GET, path, hello, NULL);
    string_free(path);
    if (!ready) {
        websrv_free(server);
        return EXIT_FAILURE;
    }
    uint16_t port = websrv_port(server);
    pid_t child = fork();
    if (child == 0) {
        int served = websrv_serve_once(server, 3000);
        websrv_free(server);
        _exit(served == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    websrv_free(server);
    if (child < 0) return EXIT_FAILURE;
    string_t *url = string_sprintf("http://127.0.0.1:%u/hello", port);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_response_t *response = client && request ? http_client_send(client, request) : NULL;
    string_t *text = http_response_ok(response) ? http_response_text(response) : NULL;
    bool ok = text != NULL;
    if (ok) string_printf("status=%ld\n%S\n", http_response_status(response), text);
    string_free(text);
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    int status = 0;
    ok = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && ok;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
