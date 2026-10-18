/**
 * @file request_flow_checks.d.ts
 * @brief Browser entry-point declaration for request continuation regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

interface Window {
    /** Run continuation ordering and failure-recovery regressions. */
    checkLabRequestFlows(): Promise<void>;
}
