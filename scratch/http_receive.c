/**
 * @file http_receive.c
 * @brief Executable README example of bounded asynchronous HTTP response ownership.
 *
 * Simulates one host-delivered binary chunk without opening a connection. Browser transports use the same
 * reserve/commit/completion sequence; network policy and asynchronous callbacks belong to their host adapter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http-t.h"

int main(void)
{
    http_receive_t *receiver = http_receive_new(1024);
    unsigned char *chunk = http_receive_reserve(receiver, 3);
    if (!chunk) { http_receive_free(receiver); return EXIT_FAILURE; }
    memcpy(chunk, "ABC", 3);
    if (!http_receive_commit(receiver, 3) || !http_receive_finish(receiver, 200)) {
        http_receive_free(receiver); return EXIT_FAILURE;
    }
    printf("%u: %zu bytes\n", http_receive_status(receiver), http_receive_size(receiver));
    http_receive_free(receiver);
    return EXIT_SUCCESS;
}
