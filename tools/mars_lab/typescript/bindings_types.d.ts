/**
 * @file bindings_types.d.ts
 * @brief Typed native binding events and editor projection results.
 *
 * Defines the action-to-payload relationships consumed by WorksheetBindings and its
 * browser checks. These declaration-only contracts depend on DOM input types and
 * the shared WorksheetBinding and WorksheetBindingCard models; they create no runtime
 * globals. Native code owns action selection, validation and mathematical policy.
 */

/** Input-only payload used before an integrator binding card has been prepared. */
interface LabBindingInputPayload {
    card: {input: HTMLInputElement};
}

/** Rendered binding card associated with a native card action. */
interface LabBindingCardPayload {
    card: WorksheetBindingCard;
}

/** Integrator row identity and native edit operation, without a binding card. */
interface LabBindingRowPayload {
    card: null;
    item: HTMLElement;
    index: number;
    operation: 1 | 2 | 3;
}

/** Native action names mapped to their corresponding host payloads. */
interface LabBindingEventPayloads {
    '': LabBindingInputPayload;
    evaluate: LabBindingCardPayload;
    commit: LabBindingCardPayload;
    history: LabBindingCardPayload;
    clear: LabBindingCardPayload;
    copy: LabBindingCardPayload;
    toggle: LabBindingCardPayload;
    invalidate: LabBindingInputPayload;
    prepare: LabBindingInputPayload;
    edit: LabBindingRowPayload;
}

/** Mapped union keeps each action correlated with its own payload during dispatch. */
type LabBindingNativeAction<K extends keyof LabBindingEventPayloads = keyof LabBindingEventPayloads> = {
    [P in K]: {action: P; prevent: 0 | 1} & LabBindingEventPayloads[P]
}[K];

/** Optional handlers preserving each native action's payload relationship. */
type LabBindingEventHandlers = {
    [P in keyof LabBindingEventPayloads]?: (action: LabBindingNativeAction<P>) => unknown;
};

/** Actions supported by ordinary binding cards. */
type LabBindingCardHandlers = Pick<LabBindingEventHandlers,
    'evaluate' | 'commit' | 'history' | 'clear' | 'copy' | 'toggle'>;

/** Actions supported by integrator binding inputs and rows. */
type LabBindingIntegratorHandlers = Pick<LabBindingEventHandlers, 'invalidate' | 'prepare' | 'edit'>;

/** Tuple union rejects installation of handlers for the wrong native event family. */
type LabBindingEventInstallation =
    | [root: HTMLElement, nativeEvent: 'lab_binding_event', actions: LabBindingCardHandlers]
    | [root: HTMLElement, nativeEvent: 'lab_binding_integrator_event', actions: LabBindingIntegratorHandlers];

/** Trimmed editor text and its staged UTF-8 byte length. */
interface LabBindingEditorSnapshot {
    text: string;
    length: number;
}

/** Native compact display projection and the bindings retained alongside it. */
interface LabBindingCompactEditor {
    display: string;
    bindings: WorksheetBinding[];
    shortened: boolean;
}

/** Native goal-seek expression and symbolic starting values keyed by binding name. */
interface LabBindingGoalStart {
    expression: string;
    start: Record<string, string>;
}
