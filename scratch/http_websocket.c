#include "http.h"
#include "array.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
        return EXIT_FAILURE;
    string_t *url = string_new_with(argv[1]);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_websocket_t *socket = client && request ? http_websocket_open(client, request) : NULL;
    bool text = false;
    bool sent = socket && http_websocket_send(socket, true, "MARS", 4);
    array_t *reply = sent ? http_websocket_receive(socket, &text) : NULL;
    string_t *output = string_new();
    bool ok = reply && text && output && !string_append_utf8_exact(output, array_get(reply, 0), array_size(reply));
    if (ok)
        string_printf("WebSocket echo: %S\n", output);
    if (http_websocket_is_open(socket))
        ok = http_websocket_close(socket, 1000, NULL) && ok;
    string_free(output);
    array_destroy(reply);
    http_websocket_free(socket);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
