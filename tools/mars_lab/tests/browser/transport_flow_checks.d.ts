/**
 * @file transport_flow_checks.d.ts
 * @brief Transport continuation frames and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

interface Window { checkLabTransportFlows(): Promise<void>; }
interface LabTransportTestFrame {
    operation?: string;
    callback?: (request: unknown) => unknown;
    request?: object | null;
    payload?: object;
    fallback?: string;
    result?: unknown;
}
