/**
 * @file binding_test_types.d.ts
 * @brief Shared fixture contracts for binding, form, widget and event regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points installed by the binding, forms and event browser checks. */
interface Window {
    /** @brief Check native binding controls and their deferred browser effects. */
    checkLabBindingDom: () => Promise<unknown>;

    /** @brief Check binding continuations and controller resource ownership. */
    checkLabBindingFlows: () => Promise<unknown>;

    /** @brief Check accepted commits, cache projection and stale completions. */
    checkLabBindingCommit: () => Promise<unknown>;

    /** @brief Check request normalisation and accepted edit projection. */
    checkLabBindingSync: () => Promise<unknown>;

    /** @brief Check native form state and browser adapters. */
    checkLabForms: () => Promise<unknown>;

    /** @brief Check structured request payloads. */
    checkLabPayload: () => Promise<unknown>;

    /** @brief Check request preparation and completion guards. */
    checkLabGoal: () => Promise<unknown>;

    /** @brief Check browser event routing. */
    checkLabEvents: () => unknown;

    /** @brief Check widget subscriptions and service arguments. */
    checkLabWidgetEvents: () => unknown;

    /** @brief Check selection-menu subscriptions and keyboard events. */
    checkLabSelectEvents: () => unknown;
}

/** @brief Native service plan with the ordered calls inspected by these fixtures. */
interface BindingTestPlan extends NativeServicePlan {
    calls: MarsTransportServiceCall[];
    result?: unknown;
}

/** @brief Live controls and visible date retained by a widget event fixture. */
interface BindingWidgetContext {
    input: HTMLInputElement | null;
    button: Element | null;
    shell: Element | null;
    year: number;
    month: number;
}

/** @brief Request fields inspected by the binding commit checks. */
interface BindingCommitPayload {
    action: string;
    operation: string;
    mode: string;
    text: string;
    unset_constants: boolean;
    bindings: unknown[];
}

/** @brief Controls belonging to one enhanced selection-menu fixture. */
interface BindingSelectFixture {
    select: HTMLSelectElement;
    shell: HTMLElement;
    label: HTMLLabelElement;
    options: {searchable: boolean; details: boolean};
    button: HTMLButtonElement;
    menu: HTMLElement;
    search: HTMLInputElement;

    /** @brief Read the current option nodes after any menu rebuild. */
    items(): HTMLButtonElement[];
}

/** @brief Normalised record passed to the binding service. */
interface BindingSyncRecord {
    name: string;
    kind: unknown;
    value: string;
}

/** @brief Native request assembled from an editor body and binding records. */
interface BindingSyncRequest {
    action: string;
    operation: string;
    mode: string;
    text: string;
    bindings: BindingSyncRecord[];
}

/** @brief Prepared source and request returned by native normalisation. */
interface BindingSyncPlan {
    source: string;
    request: BindingSyncRequest;
    invalid?: boolean;
}

/** @brief Deferred completion for DOM fixtures that exercise several response shapes. */
interface BindingDomGate {
    promise: Promise<unknown>;

    /** @brief Release the pending fixture operation. */
    resolve(value?: unknown): void;
}

/** @brief Deferred source update or completion signal for commit fixtures. */
interface BindingCommitGate {
    promise: Promise<string | undefined>;

    /** @brief Release the pending update, optionally supplying a source. */
    resolve(value?: string): void;
}

/** @brief Minimal editor response used by controlled presentation requests. */
interface BindingEditorResponse {
    editor: {expression: string};
}

/** @brief Controlled presentation request with explicit success and failure paths. */
interface BindingSyncGate {
    promise: Promise<BindingEditorResponse>;
    request?: BindingSyncRequest;

    /** @brief Complete the request with a native editor response. */
    resolve(value: BindingEditorResponse): void;

    /** @brief Reject the request without changing the supplied failure. */
    reject(reason?: unknown): void;
}

/** @brief Existing editor cache entry retained for fixture cleanup. */
interface BindingEditorCacheEntry {
    key: string;
    present: boolean;
    value: WorksheetEditorMetadata | undefined;
}

/** @brief DOM state retained while a widget fixture replaces children and attributes. */
interface BindingElementSnapshot {
    element: LabWorksheetTestSnapshotElement;
    children: ChildNode[];
    attributes: [string, string][];
    value: string | undefined;
}
