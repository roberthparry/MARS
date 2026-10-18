/**
 * @file memory_checks.ts
 * @brief Shared browser and codec checks for statically linked memory operations.
 *
 * Uses caller-owned staging storage to check byte conversion, unaligned copies,
 * overlap in either direction, unsigned comparison, canaries and zero-length calls.
 */

/** @brief Linear memory and the four retained C memory entry points. */
interface LabMemoryTestNative {
    /** @brief Borrow the instance's linear memory. */
    memory: WebAssembly.Memory;
    /** @brief Fill bytes and return the destination address. */
    memset(destination: number, value: number, length: number): number;
    /** @brief Copy disjoint byte ranges and return the destination address. */
    memcpy(destination: number, source: number, length: number): number;
    /** @brief Copy possibly overlapping byte ranges and return the destination address. */
    memmove(destination: number, source: number, length: number): number;
    /** @brief Compare ranges using unsigned byte ordering. */
    memcmp(left: number, right: number, length: number): number;
}

interface Window {
    /** @brief Check memory functions using at least 12,290 writable bytes from buffer. */
    checkLabMemory(native: LabMemoryTestNative, buffer: number): void;
}

/** @brief Verify memory operations in either real WASM instance without relying on a host shim. */
window.checkLabMemory = function checkLabMemory(native: LabMemoryTestNative, buffer: number): void {
    /** @brief Report a failed memory primitive assertion. */
    function check(condition: boolean, message: string): void {
        if (!condition) throw new Error('WASM memory: ' + message);
    }
    // Exercise actual WASM runtime primitives, not host shims, including unaligned ranges and canaries.
    const memory = new Uint8Array(native.memory.buffer);
    for (const name of ['memset', 'memcpy', 'memmove', 'memcmp'] as const)
        check(typeof native[name] === 'function', `missing native ${name}`);
    for (const length of [0, 1, 3, 31, 256, 4096]) {
        const source = buffer + 1;
        const target = buffer + 8193;
        memory.fill(0x5a, buffer, target + length + 1);
        check(native.memset(source, 0x1ab, length) === source, 'memset return pointer');
        check(memory.subarray(source, source + length).every(byte => byte === 0xab), 'memset byte conversion');
        check(memory[source - 1] === 0x5a && memory[source + length] === 0x5a, 'memset touched a canary');
        check(native.memcpy(target, source, length) === target, 'memcpy return pointer');
        check(memory.subarray(target, target + length).every(byte => byte === 0xab), 'memcpy contents');
        check(memory[target - 1] === 0x5a && memory[target + length] === 0x5a, 'memcpy touched a canary');
        check(native.memcmp(source, target, length) === 0, 'memcmp equal ranges');
        if (length) {
            memory[target + length - 1] = 0x7f;
            check(native.memcmp(source, target, length) > 0, 'memcmp unsigned-byte ordering');
            check(native.memcmp(target, source, length) < 0, 'memcmp reverse ordering');
        }
        for (const [from, to] of [[1, 4], [4, 1], [1, 1], [1, 8193], [8193, 1]]) {
            const extent = Math.max(from, to) + length + 1;
            for (let i = 0; i < extent; ++i)
                memory[buffer + i] = i % 251;
            const expected = memory.slice(buffer, buffer + extent);
            expected.copyWithin(to, from, from + length);
            check(native.memmove(buffer + to, buffer + from, length) === buffer + to, 'memmove return pointer');
            check(
                memory.subarray(buffer, buffer + extent).every((byte, index) => byte === expected[index]),
                `memmove contents/canaries for ${from}, ${to}, ${length}`);
        }
    }
    // A one-past-memory pointer must not be dereferenced for a zero-length operation.
    const end = memory.length;
    check(native.memset(end, -1, 0) === end, 'zero-length memset accessed memory');
    check(native.memcpy(end, end, 0) === end, 'zero-length memcpy accessed memory');
    check(native.memmove(end, end, 0) === end, 'zero-length memmove accessed memory');
    check(native.memcmp(end, end, 0) === 0, 'zero-length memcmp accessed memory');
};
