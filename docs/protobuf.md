# Protocol Buffers

Include `protobuf.h` to encode and decode bounded Protocol Buffers wire
messages in native C. No external Protocol Buffers library is required.

Protocol Buffers is a **binary data format**, not a transport. The
[HTTP module](http.md) uses it for unary gRPC calls. JSON and XML remain
separate alternative formats.

## Schema and supported features

Wire records contain field numbers and wire types, not field names or a full
schema. Your application must know the service's `.proto` definition.
This module does not compile schemas, generate clients, perform reflection,
or implement ProtoJSON.

Supported wire types are varints, fixed-width 32-bit and 64-bit values, and
length-delimited bytes. Length-delimited fields can hold UTF-8 strings,
nested messages or packed repeated values; the schema determines which.
Nested messages are decoded explicitly by calling `protobuf_decode` on the
borrowed field bytes. Packed scalar arrays require application interpretation.
Deprecated group wire types are rejected.

Ordered records preserve repeated field numbers and unknown fields.
Indexed access is constant-time; there is no implicit first/last-value choice,
field merging or schema default insertion. Varints are re-encoded at their
shortest length, so decoding and re-encoding is not guaranteed to preserve the
original byte spelling. It is not a canonical serialisation for signing.

Numeric access returns raw unsigned bits. For `sint64`, use
`protobuf_zigzag_encode` and `protobuf_zigzag_decode`; ordinary negative
`int64` values use their two's-complement unsigned representation instead.
For fixed-width floating-point fields, copy IEEE-754 bits using `memcpy`
rather than aliasing a pointer. Strings are byte fields: validate/decode UTF-8
with `string_t` before treating them as text.

## Ownership, limits and errors

Builders and decoders return opaque owned `protobuf_t` handles; release them
with `protobuf_free`. Added byte fields are copied. `protobuf_encode`
returns an owned byte `array_t`, released with `array_destroy`.
Borrowed field bytes remain valid until the message is freed.

The defaults are 16 MiB encoded data and 4,096 field occurrences. Explicit
limits may be up to 64 MiB and 1,048,576 occurrences. These are payload limits,
not total process-memory caps. Decoder budgets include the supplied encoded
input, even when it uses unnecessarily long varints. Malformed tags, integer
overflow, truncation, unsupported wire types and exceeded limits fail with
NULL; failed builder additions leave the message unchanged. Empty messages
and empty byte fields are valid.

## Runnable example

This is also `scratch/protobuf_roundtrip.c`. The field numbers correspond to
an application schema with an unsigned integer field 1 and string field 2.

```c
#include "protobuf.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

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
    if (ok) {
        printf("Field %" PRIu32 ": %" PRIu64 "\n", protobuf_field(decoded, 0), value);
        printf("Field %" PRIu32 ": %.*s\n", protobuf_field(decoded, 1), (int)size, name);
        printf("Encoded bytes: %zu\n", array_size(encoded));
    }
    protobuf_free(decoded);
    array_destroy(encoded);
    protobuf_free(message);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Output:

```text
Field 1: 150
Field 2: MARS
Encoded bytes: 9
```

## Public API

### `protobuf_new`

Create an empty bounded message.

`protobuf_t *protobuf_new(size_t max_bytes, size_t max_fields);`

- `max_bytes`: Encoded-byte budget; zero selects 16 MiB; maximum 64 MiB.

- `max_fields`: Field-count budget; zero selects 4096; maximum 1048576.

Returns: Owned message, or NULL; release with protobuf_free.

### `protobuf_free`

Release a message and copied byte fields.

`void protobuf_free(protobuf_t *message);`

- `message`: Owned message; NULL is harmless.

### `protobuf_decode`

Decode wire records without guessing schema types; reject truncation, overflow and groups.

`protobuf_t *protobuf_decode(const void *data, size_t size, size_t max_bytes, size_t max_fields);`

- `data`: Borrowed binary bytes; NULL only for size zero.

- `size`: Encoded byte count.

- `max_bytes`: Encoded-byte budget, as for protobuf_new.

- `max_fields`: Field-count budget, as for protobuf_new.

Returns: Owned message, or NULL on malformed input, exceeded limits or allocation failure.

### `protobuf_encode`

Serialise ordered records with shortest-length varints.

`array_t *protobuf_encode(const protobuf_t *message);`

- `message`: Borrowed message.

Returns: Owned byte array released with array_destroy, or NULL; this is not canonical schema serialisation.

### `protobuf_add_integer`

Append an integer wire record, preserving duplicate field numbers.

`bool protobuf_add_integer(protobuf_t *message, uint32_t field, protobuf_wire_t wire, uint64_t value);`

- `message`: Message to modify.

- `field`: Field number from 1 to 536870911.

- `wire`: VARINT, FIXED32 or FIXED64; fixed values are raw bits, including floating-point representations.

- `value`: Unsigned value or bit pattern; FIXED32 rejects values exceeding UINT32_MAX.

Returns: True on success; false leaves the message unchanged.

### `protobuf_add_bytes`

Copy a length-delimited field, including nested messages or packed fields.

`bool protobuf_add_bytes(protobuf_t *message, uint32_t field, const void *data, size_t size);`

- `message`: Message to modify.

- `field`: Valid non-zero field number.

- `data`: Borrowed bytes; NULL only when size is zero.

- `size`: Byte count.

Returns: True on success; false leaves the message unchanged.

### `protobuf_count`

Count ordered wire records.

`size_t protobuf_count(const protobuf_t *message);`

- `message`: Borrowed message.

Returns: Record count, or zero for NULL.

### `protobuf_field`

Read a record's field number in constant time.

`uint32_t protobuf_field(const protobuf_t *message, size_t index);`

- `message`: Borrowed message.

- `index`: Zero-based record index.

Returns: Field number, or zero if out of range.

### `protobuf_wire`

Read a record's wire type.

`int protobuf_wire(const protobuf_t *message, size_t index);`

- `message`: Borrowed message.

- `index`: Zero-based record index.

Returns: Wire type, or -1 if out of range.

### `protobuf_integer`

Read a VARINT/FIXED32/FIXED64 record without schema conversion.

`bool protobuf_integer(const protobuf_t *message, size_t index, uint64_t *value);`

- `message`: Borrowed message.

- `index`: Zero-based record index.

- `value`: Required output, unchanged on failure.

Returns: True on success, false for byte fields or invalid arguments.

### `protobuf_bytes`

Borrow a length-delimited payload.

`const void *protobuf_bytes(const protobuf_t *message, size_t index, size_t *size);`

- `message`: Borrowed message.

- `index`: Zero-based record index.

- `size`: Required output byte count; set to zero for empty or invalid fields.

Returns: Borrowed bytes until message destruction; NULL for empty or invalid fields. Check protobuf_wire to distinguish.

### `protobuf_zigzag_encode`

Convert a signed sint64 value to its unsigned ZigZag wire representation.

`uint64_t protobuf_zigzag_encode(int64_t value);`

- `value`: Signed value.

Returns: ZigZag representation.

### `protobuf_zigzag_decode`

Decode an unsigned ZigZag sint64 representation.

`int64_t protobuf_zigzag_decode(uint64_t value);`

- `value`: Encoded value.

Returns: Signed value, including INT64_MIN.

## Tests and format reference

Run `make -j1 test_protobuf`. Tests cover wire-type round trips, duplicate
records, empty messages, exact bytes, malformed varints, invalid tags,
truncation, field/byte limits and ZigZag boundary values. The README example
runs after the ordinary tests.

See the official [wire-format specification](https://protobuf.dev/programming-guides/encoding/)
for schema-independent encoding details.
