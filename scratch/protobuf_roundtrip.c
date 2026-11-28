/**
 * @file protobuf_roundtrip.c
 * @brief Runnable Protocol Buffers round-trip example.
 *
 * Constructs a bounded message, encodes it, decodes it and checks retained field values. The documented program
 * demonstrates ownership and cleanup without requiring a network connection.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "format-t.h"
#include "protobuf-t.h"

int main(void)
{
    protobuf_t *message = protobuf_new(1024, 16);
    bool ok =
        message && protobuf_add_integer(message, 1, PROTOBUF_VARINT, 150) && protobuf_add_bytes(message, 2, "MARS", 4);
    array_t *encoded = ok ? protobuf_encode(message) : NULL;
    protobuf_t *decoded = encoded ? protobuf_decode(array_get(encoded, 0), array_size(encoded), 1024, 16) : NULL;
    uint64_t value = 0;
    size_t size = 0;
    const char *name = decoded ? protobuf_bytes(decoded, 1, &size) : NULL;
    ok = decoded && protobuf_integer(decoded, 0, &value) && name && size == 4;
    string_t *name_text = ok ? string_new() : NULL;
    ok = ok && name_text && string_append_bytes(name_text, name, size) == 0;
    if (ok) {
        fmt_print("Field {field}: {value}\n", FMT(field, protobuf_field(decoded, 0)), FMT(value));
        fmt_print("Field {field}: {name_text}\n", FMT(field, protobuf_field(decoded, 1)), FMT(name_text));
        fmt_print("Encoded bytes: {encoded_bytes}\n", FMT(encoded_bytes, array_size(encoded)));
    }
    string_free(name_text);
    protobuf_free(decoded);
    array_destroy(encoded);
    protobuf_free(message);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
