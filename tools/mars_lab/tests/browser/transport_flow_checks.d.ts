/**
 * @file transport_flow_checks.d.ts
 * @brief Transport continuation frames and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

interface Window {
    /** Run transport ordering, cancellation and cleanup regressions. */
    checkLabTransportFlows(): Promise<void>;
}

/** Mutable continuation frame passed to the native transport fixture. */
interface LabTransportTestFrame {
    operation?: string;
    callback?: (request: unknown) => unknown;
    request?: object | null;
    payload?: object;
    fallback?: string;
    result?: unknown;
}

/** Response and decoded data returned by a completed fixture. */
interface LabTransportTestResult {
    response: unknown;
    data: unknown;
}
