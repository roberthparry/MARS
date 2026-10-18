/**
 * @file function_checks.d.ts
 * @brief Browser entry-point declaration for function-card regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** @brief Original attribute restored after a function-card scenario. */
interface LabFunctionAttribute {
    name: string;
    value: string;
}

/** @brief DOM state retained without replacing the original child nodes. */
interface LabFunctionNodeSnapshot {
    node: HTMLElement;
    children: ChildNode[];
    attributes: LabFunctionAttribute[];
}

/** @brief Suspended request released before the fixture restores its mocks. */
interface LabFunctionPendingRequest {
    task: Promise<unknown>;
    gate: LabTestResponseGate;
}

/** @brief Expected output indexed directly by the scenario name. */
interface LabFunctionExpectedOutputs {
    [scenario: string]: string;
}

interface Window {
    /** @brief Check function-card projection and asynchronous request ownership. */
    checkLabFunction(): Promise<void>;
}
