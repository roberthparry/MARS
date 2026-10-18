/**
 * @file request_checks.d.ts
 * @brief Browser entry-point declaration for request ownership regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

interface Window {
    /** Run request ownership and deferred-fetch regressions. */
    checkLabRequests(): Promise<void>;
}

/** Pending network completion and its borrowed cancellation signal. */
interface LabRequestPendingResponse {
    /** Complete the controlled fetch with its original response object. */
    resolve(value: Response): void;

    /** Reject the controlled fetch with the supplied reason. */
    reject(reason: unknown): void;

    signal: AbortSignal;
}

/** Captured request fields inspected at the binary transport boundary. */
interface LabRequestSentBody {
    action?: string;
    persist_expression?: boolean;
    transient?: boolean;
}

/** Captured URL and decoded body from one fixture request. */
interface LabRequestSent {
    url: string;
    body: LabRequestSentBody;
}
