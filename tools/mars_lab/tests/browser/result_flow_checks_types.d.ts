/**
 * @file result_flow_checks_types.d.ts
 * @brief Result-action continuation plan and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabResultFlows(): Promise<void>;
}

interface ResultFlowFixturePlan {
    calls: {service: string; args: unknown[]}[];
    wait: boolean;
}
