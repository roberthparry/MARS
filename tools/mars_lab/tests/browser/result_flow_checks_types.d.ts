/**
 * @file result_flow_checks_types.d.ts
 * @brief Result-action continuation plan and browser entry-point fixtures.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Check native action sequencing and failure recovery. */
    checkLabResultFlows(): Promise<void>;
}

/** Native continuation effects inspected without executing them. */
interface ResultFlowFixturePlan {
    calls: {service: string; args: unknown[]}[];
    wait: boolean;
}

/** Native widget plan with the control and labels used by these checks. */
interface ResultFlowWidgetPlan {
    calls: ResultFlowWidgetCall[];
}

/** Arguments passed to one native-selected control enhancement. */
interface ResultFlowWidgetCall {
    args: [HTMLSelectElement, ResultFlowWidgetOptions];
}

/** Optional labels inspected by the widget bootstrap check. */
interface ResultFlowWidgetOptions {
    searchPlaceholder?: string;
    placeholder?: string;
}
