/**
 * @file binding_test_types.d.ts
 * @brief Shared fixture contracts for binding, form, widget and event regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points installed by the binding, forms and event browser checks. */
interface Window {
    checkLabBindingDom: () => Promise<unknown>;
    checkLabBindingFlows: () => Promise<unknown>;
    checkLabBindingCommit: () => Promise<unknown>;
    checkLabBindingSync: () => Promise<unknown>;
    checkLabForms: () => Promise<unknown>;
    checkLabPayload: () => Promise<unknown>;
    checkLabGoal: () => Promise<unknown>;
    checkLabEvents: () => unknown;
    checkLabWidgetEvents: () => unknown;
    checkLabSelectEvents: () => unknown;
}
interface BindingTestPlan extends NativeServicePlan {
    calls: {service: string; args: unknown[]}[];
    result?: unknown;
}
interface BindingWidgetContext {
    input: HTMLInputElement | null;
    button: Element | null;
    shell: Element | null;
    year: number;
    month: number;
}
interface BindingCommitPayload {
    action: string;
    operation: string;
    mode: string;
    text: string;
    unset_constants: boolean;
    bindings: unknown[];
}
interface BindingSelectFixture {
    select: HTMLSelectElement;
    shell: HTMLElement;
    label: HTMLLabelElement;
    options: {searchable: boolean; details: boolean};
    button: HTMLButtonElement;
    menu: HTMLElement;
    search: HTMLInputElement;
    items(): HTMLButtonElement[];
}
interface BindingSyncRecord { name: string; kind: unknown; value: string; }
interface BindingSyncRequest {
    action: string; operation: string; mode: string; text: string;
    bindings: BindingSyncRecord[];
}
interface BindingSyncPlan { source: string; request: BindingSyncRequest; invalid?: boolean; }
