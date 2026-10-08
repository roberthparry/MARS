/**
 * @file http_grpc.c
 * @brief Runnable unary gRPC client example.
 *
 * Builds a Protocol Buffers payload, sends it to the supplied endpoint and validates the unary reply. It
 * demonstrates the HTTP and protobuf APIs together without generated service stubs.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "protobuf.h"

int main(int argc, char **argv)
{
    if (argc != 2)
        return EXIT_FAILURE;
    string_t *url = string_new_with(argv[1]);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    protobuf_t *message = protobuf_new(1024, 16);
    bool ready = client && request && message && protobuf_add_integer(message, 1, PROTOBUF_VARINT, 150) &&
                 http_request_set_grpc(request, message);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    http_grpc_t *result = response ? http_response_grpc(response, 16) : NULL;
    uint64_t value = 0;
    bool ok = http_grpc_status(result) == 0 && protobuf_integer(http_grpc_payload(result), 0, &value);
    if (ok)
        printf("gRPC status: %d\nReply: %" PRIu64 "\n", http_grpc_status(result), value);
    http_grpc_free(result);
    http_response_free(response);
    protobuf_free(message);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
