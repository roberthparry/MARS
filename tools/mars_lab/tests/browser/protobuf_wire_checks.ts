/**
 * @file protobuf_wire_checks.ts
 * @brief Shared wire-core regressions through both page and worker codec adapters.
 *
 * Unknown fields exercise the complete numeric wire range without changing the
 * browser value schema. Malformed fields must reject the enclosing message and
 * leave the codec able to decode the next valid message.
 */
interface Window {
    /** @brief Check bounded wire parsing in one codec instance. */
    checkLabProtobufWire(codec: LabWireTestCodec): void;
}

/** @brief The only browser capability needed by these wire fixtures. */
interface LabWireTestCodec {
    /** @brief Decode a borrowed binary envelope or throw on malformed input. */
    decode(input: ArrayBuffer): unknown;
}

/** @brief Run identical accepted and rejected wire vectors against either WASM binary. */
window.checkLabProtobufWire = function checkLabProtobufWire(codec: LabWireTestCodec): void {
    const empty = [8, 1, 18, 2, 8, 5];
    const accepted = [
        [120, 255, 255, 255, 255, 255, 255, 255, 255, 255, 1], // Full uint64 varint.
        [120, 128, 0], // Non-minimal but valid zero.
        [121, 239, 205, 171, 137, 103, 69, 35, 1], // Fixed64.
        [125, 239, 205, 171, 137], // Fixed32.
        [122, 0], // Empty bytes.
        [122, 3, 0, 255, 42], // Opaque bytes.
        [248, 255, 255, 255, 15, 0] // Maximum field number.
    ];
    for (const field of accepted) {
        const decoded = codec.decode(new Uint8Array([...empty, ...field]).buffer);
        if (!decoded || typeof decoded !== 'object' || Object.keys(decoded).length !== 0)
            throw new Error('Shared wire core changed the empty-object envelope');
    }
    const rejected = [
        [0], [120, 128],
        [120, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2], // Overflow.
        [120, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0], // More than ten bytes.
        [121, 1], [125, 1], [122, 5, 1],
        [122, 255, 255, 255, 255, 255, 255, 255, 255, 255, 1], // Length cannot fit input.
        [123], [124], [126], [127], // Groups and unsupported types.
        [128, 128, 128, 128, 16, 0] // Field number exceeds 29 bits.
    ];
    for (const field of rejected) {
        let failed = false;
        try {
            codec.decode(new Uint8Array([...empty, ...field]).buffer);
        } catch {
            failed = true;
        }
        if (!failed) throw new Error('Shared wire core accepted malformed field: ' + field.join(','));
    }
    codec.decode(new Uint8Array(empty).buffer);
};
