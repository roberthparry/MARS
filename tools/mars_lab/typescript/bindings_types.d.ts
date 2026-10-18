/**
 * @file bindings_types.d.ts
 * @brief Binding dependency contracts, native events and editor projection results.
 *
 * Defines the narrow browser capabilities and action-to-payload relationships consumed
 * by WorksheetBindings and its browser checks. These declaration-only contracts depend
 * on DOM input types and the shared WorksheetBinding and WorksheetBindingCard models;
 * they create no runtime globals. Shared browser services and panel state are declared
 * once, followed by the five controller dependency lists. Native code owns action
 * selection, validation and mathematical policy.
 */

// Native event payloads. The action name selects one of three concrete shapes.

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

/** @brief Native action on a rendered binding card. */
interface LabBindingCardAction extends LabBindingCardPayload {
    action: 'evaluate' | 'commit' | 'history' | 'clear' | 'copy' | 'toggle'; /**< Selected card operation. */
    prevent: 0 | 1; /**< Whether the browser must cancel the original event. */
}

/** @brief Native action on an integrator input before card preparation. */
interface LabBindingInputAction extends LabBindingInputPayload {
    action: '' | 'invalidate' | 'prepare'; /**< Selected input operation, or no selected operation. */
    prevent: 0 | 1; /**< Whether the browser must cancel the original event. */
}

/** @brief Native edit action identifying an integrator row. */
interface LabBindingRowAction extends LabBindingRowPayload {
    action: 'edit'; /**< Row editing is the sole operation carrying a row payload. */
    prevent: 0 | 1; /**< Whether the browser must cancel the original event. */
}

/** @brief Native actions discriminated by their card, input or row operation names. */
type LabBindingNativeAction = LabBindingCardAction | LabBindingInputAction | LabBindingRowAction;

// Delegated event handlers. Return values are deliberately ignored by the router.

/** @brief Optional handlers for ordinary binding cards. */
interface LabBindingCardHandlers {
    /** @brief Evaluate the selected card. */
    evaluate?(action: LabBindingCardAction): unknown;

    /** @brief Commit the selected card's input. */
    commit?(action: LabBindingCardAction): unknown;

    /** @brief Refresh history controls for a card event. */
    history?(action: LabBindingCardAction): unknown;

    /** @brief Clear the selected card. */
    clear?(action: LabBindingCardAction): unknown;

    /** @brief Copy the selected card's value. */
    copy?(action: LabBindingCardAction): unknown;

    /** @brief Toggle the selected binding's kind. */
    toggle?(action: LabBindingCardAction): unknown;
}

/** @brief Optional handlers for integrator inputs and row edits. */
interface LabBindingIntegratorHandlers {
    /** @brief Invalidate metadata after an input change. */
    invalidate?(action: LabBindingInputAction): unknown;

    /** @brief Prepare an integrator binding input. */
    prepare?(action: LabBindingInputAction): unknown;

    /** @brief Apply an integrator row edit. */
    edit?(action: LabBindingRowAction): unknown;
}

/** @brief Combined handlers used by the delegated native event router. */
interface LabBindingEventHandlers extends LabBindingCardHandlers, LabBindingIntegratorHandlers {
    /** @brief Handle an input event with no selected native operation. */
    ''?(action: LabBindingInputAction): unknown;
}

/** Tuple union rejects installation of handlers for the wrong native event family. */
type LabBindingEventInstallation =
    | [root: HTMLElement, nativeEvent: 'lab_binding_event', actions: LabBindingCardHandlers]
    | [root: HTMLElement, nativeEvent: 'lab_binding_integrator_event', actions: LabBindingIntegratorHandlers];

// Results returned by native editor and goal-source preparation.

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
    start: {[name: string]: string};
}

/** @brief Explicit capabilities borrowed by binding controllers from their owners. */
declare namespace WorksheetBindings {
    // Shared browser capabilities.

    /** @brief Native binding projection and event classification. */
    interface BindingProjectionNative {
        /** @brief Invoke a native entry point; the caller casts its documented result. */
        call(entry: string, ...args: unknown[]): unknown;
    }

    /** @brief Native binding projection and synchronous browser-service dispatch. */
    interface BindingNative extends BindingProjectionNative {
        /** @brief Execute the native-selected services using the supplied registry. */
        services(plan: NativeServicePlan, registry: NativeServiceRegistry | null): unknown;
    }

    /** @brief Native operations common to the editor, panels, goal and commit controller. */
    interface BindingRuntime {
        readonly dom: BindingNative; /**< Scoped native calls and browser-service dispatch. */

        /** @brief Advance a native workflow; its caller names the expected result. */
        flow(kind: number, frame: object): Promise<unknown>;
    }

    /** @brief Ordinary binding controls used by integrator and editor projections. */
    interface BindingControls {
        /** @brief Render the supplied native bindings. */
        renderVariableValues(bindings: WorksheetBinding[]): void;

        /** @brief Clear rendered binding controls. */
        clearVariableValues(): void;
    }

    /** @brief Panel values used by goal preparation and editor synchronisation. */
    interface BindingValues {
        currentVariables: string[]; /**< Names available for goal seeking and derivative controls. */
        bindingValueCache: Map<string, string>; /**< Authored values indexed by binding name. */
    }

    /** @brief Full panel state updated by the binding controller. */
    interface BindingPanelState extends BindingControls, BindingValues {
        currentDifferentiable: boolean; /**< Whether the native expression is differentiable. */
    }

    /** @brief Evaluation view fields consumed by native editor projection. */
    interface BindingEvaluationView {
        readonly caches: { // Shared editor metadata retained by the result owner.
            readonly editors: Map<string, WorksheetEditorMetadata>; /**< Native editor metadata indexed by source text. */
        };

        readonly editors: Map<string, WorksheetEditorMetadata>; /**< Direct editor-cache access for native evaluation. */
        readonly fullText: string; /**< Full authored expression. */
        readonly rawEditor: LabBindingEditorSnapshot; /**< Trimmed editor text and staged byte length. */
        readonly bodyText: string; /**< Current editor body. */
        readonly lastInput: string; /**< Most recently evaluated input. */
    }

    /** @brief Request admission and lifetime operations used by binding commits and refreshes. */
    interface BindingRequests {
        /** @brief Admit an operation with the request owner's existing options. */
        begin(operation: string, mode?: string, options?: WorksheetRequests.MarsApiOptions): WorksheetRequest | null;

        /** @brief Release the supplied request's resources. */
        finish(request: WorksheetRequest | null | undefined): void;

        /** @brief Run admitted work; callers cast results at their consuming boundary. */
        run(operation: string, mode: string, callback: (request: WorksheetRequest) => unknown,
            options?: WorksheetRequests.MarsApiOptions): Promise<unknown>;

        /** @brief Cancel the channel associated with an operation. */
        cancel(operation: string): void;
    }

    // Dependencies: one list per browser owner, with native calls inherited above.

    /** @brief Capabilities required to project and assemble editor expressions. */
    interface EditorDependencies extends BindingRuntime {
        readonly view: BindingEvaluationView; /**< Native evaluation view and shared metadata cache. */

        readonly workspaceMode: { // Current worksheet mode supplied by its session owner.
            /** @brief Read the active mode. */
            currentMode(): WorksheetMode;
        };
    }

    /** @brief Capabilities required to render binding cards and delegate their actions. */
    interface PanelDependencies extends BindingRuntime {
        readonly container: HTMLElement; /**< Root containing ordinary binding cards. */

        readonly bindingController: { // Binding mutations owned by the controller.
            /** @brief Serialise commits for the selected input. */
            queueBindingInputCommit(input: HTMLInputElement): Promise<unknown>;

            /** @brief Change a binding's native kind. */
            toggleBindingKind(binding: WorksheetBinding): Promise<void>;
        };

        readonly workspacePresentation: { // History presentation owned by the workspace.
            /** @brief Refresh history navigation controls. */
            updateHistoryButtons(): void;
        };
    }

    /** @brief Capabilities required to render integrator rows and apply binding state. */
    interface IntegratorPanelDependencies extends BindingRuntime {
        readonly modeEditors: WorksheetModeEditors; /**< Authored editor text stored per worksheet mode. */
        readonly container: HTMLElement; /**< Root containing integrator binding rows. */
        readonly view: BindingEvaluationView; /**< Native evaluation view and editor metadata. */

        readonly workspaceIntegrator: { // Bound names supplied by the workspace integrator.
            /** @brief Read names bound by the active integrator rows. */
            currentIntegratorBoundNames(): Set<string>;
        };

        readonly forms: { // Mutable form metadata and native revision ownership.
            /** @brief Advance the form revision. */
            nextRevision(): number;

            metadata: WorksheetRowsMetadata | null; /**< Cached native metadata, cleared after input changes. */
        };

        readonly bindingPanel: BindingControls; /**< Ordinary binding controls updated by integrator projection. */

        readonly bindingController: { // Editor projection owned by the binding controller.
            /** @brief Install expression text and its optional native presentation data. */
            setExpressionEditor(fullText: string, evaluatedBindings?: WorksheetBinding[] | null,
                                editorBodyText?: string | null, evaluationReady?: unknown): void;
        };
    }

    /** @brief Capabilities required to prepare goal sources and starting values. */
    interface GoalDependencies extends BindingRuntime {
        readonly input: HTMLTextAreaElement; /**< Authored expression input. */
        readonly wire: WorksheetNativeExports; /**< Native source resolution export. */

        readonly bindingController: { // Browser services used by native binding plans.
            /** @brief Return the controller's browser capability registry. */
            labBindingFlowServices(): NativeServiceRegistry;
        };

        readonly buffer: WorksheetNativeText; /**< UTF-8 transfer owned by the workspace buffer. */

        readonly bindingEditor: { // Native editor metadata lookup.
            /** @brief Read metadata for the supplied expression. */
            labEditorData(text: string | null): WorksheetEditorMetadata | null;
        };

        readonly bindingPanel: BindingValues; /**< Current variables and authored values retained by the binding panel. */
    }

    /** @brief Capabilities required to sequence binding commits, refreshes and projections. */
    interface ControllerDependencies extends BindingRuntime {
        readonly input: HTMLTextAreaElement; /**< Authored expression input. */
        readonly sources: WorksheetSources; /**< Native-backed expression sources. */
        readonly modeEditors: WorksheetModeEditors; /**< Authored editor text stored per worksheet mode. */
        readonly container: HTMLElement; /**< Root containing ordinary binding inputs. */
        readonly integratorContainer: HTMLElement; /**< Root containing integrator binding rows. */
        readonly defaultDatetimeText: string; /**< Default datetime editor text used by native refresh policy. */

        /** @brief Schedule owned refresh or feedback work. */
        setTimer(callback: () => void, delay: number): number;

        /** @brief Cancel an owned timer. */
        clearTimer(timer: number): void;

        readonly requests: BindingRequests; /**< Binding request admission and lifetime management. */
        readonly wire: WorksheetNativeExports; /**< Native source comparison export. */

        readonly requestPresentation: { // Native editor preparation and presentation requests.
            /** @brief Prepare metadata for editor text. */
            prepareLabEditor(text: string): Promise<WorksheetEditorMetadata>;

            /** @brief Request native presentation under optional request ownership. */
            requestLabPresentation(payload: unknown, request?: WorksheetRequest | null): Promise<WorksheetRequests.MarsApiData>;
        };

        readonly evaluationTransport: { // Evaluation transport used when refreshing bindings.
            /** @brief Fetch evaluation data with the owner's optional calculus and binding inputs. */
            fetchEvaluation(text: string, wrt?: string, action?: string, bindingSource?: unknown,
                            bindingValue?: string, request?: WorksheetRequest | null,
                            preview?: boolean): Promise<WorksheetRequests.MarsApiPostResult>;
        };

        readonly persistence: { // Worksheet persistence supplied by its owner.
            /** @brief Save authored worksheet text with the existing save options. */
            saveWorksheet(mode: WorksheetMode, text?: string, options?: WorksheetSaveOptions): void;
        };

        readonly results: { // Clipboard and copy-feedback presentation.
            /** @brief Write the selected binding text to the clipboard. */
            writeClipboardText(text: string): Promise<void>;

            /** @brief Present copy success or failure on the selected button. */
            flashCopyButton(button: WorksheetCopyButton, success: boolean): void;
        };

        readonly bindingEditor: { // Native editor metadata, formatting and binding assembly.
            /** @brief Change a named binding's kind through native preparation. */
            replaceBindingKindInExpression(sourceExpression: string, targetName: string, nextKind: string): Promise<string>;

            /** @brief Read compact editor text and retained bindings. */
            compactExpressionForEditor(fullText: string): LabBindingCompactEditor;

            /** @brief Reconcile authored, evaluated and visible binding values. */
            bindingsWithAuthoredValues(bindings: WorksheetBinding[], sourceExpression: string,
                                      visibleBindings?: WorksheetBinding[]): WorksheetBinding[];

            /** @brief Read the native editor body. */
            expressionBodyForEditor(text: string): string;

            /** @brief Read the current expression source. */
            currentExpressionText(): string;

            /** @brief Read cached native editor metadata. */
            labEditorData(text: string | null): WorksheetEditorMetadata | null;

            /** @brief Read metadata for an expression containing bindings. */
            bindingParts(text: string): WorksheetEditorMetadata | null;
        };

        readonly workspaceBindingSelection: { // Native selection of bindings and variable names.
            /** @brief Select bindings visible in the current mode. */
            visibleBindingsForCurrentMode(bindings: WorksheetBinding[]): WorksheetBinding[];

            /** @brief Read variable names from native bindings. */
            variableNamesFromBindings(bindings: WorksheetBinding[]): string[];
        };

        readonly workspaceIntegrator: { // Integrator rows and their native form preparation.
            /** @brief Read names bound by the active integrator rows. */
            currentIntegratorBoundNames(): Set<string>;

            /** @brief Refresh optional replacement rows and expression text. */
            refreshIntegratorForms(rows?: WorksheetIntegratorRow[], expression?: string): Promise<WorksheetIntegratorRow[] | null | undefined>;

            /** @brief Read current native integrator rows. */
            currentIntegratorRows(): WorksheetIntegratorRow[];
        };

        readonly workspaceMode: { // Worksheet mode ownership and evaluation entry points.
            /** @brief Save the active mode's authored editor state. */
            saveCurrentModeEditorState(): void;

            /** @brief Start evaluation using keyboard-action policy. */
            evaluateFromKeyboard(): void;

            /** @brief Resolve a mode value to its native identifier. */
            workspaceModeId(mode: unknown): number;

            /** @brief Read the active mode. */
            currentMode(): WorksheetMode;
        };

        readonly workspaceLayout: { // Editor layout scheduling.
            /** @brief Schedule the editor resize grip update. */
            scheduleEditorResizeGrip(): void;
        };

        readonly workspacePresentation: { // Workspace controls and status presentation.
            /** @brief Render derivative controls for the supplied variables. */
            renderDerivativeButtons(variables: string[]): void;

            /** @brief Refresh history navigation controls. */
            updateHistoryButtons(): void;

            /** @brief Replace the worksheet status text. */
            setStatus(text: string): void;
        };

        readonly forms: { // Native integrator row editing.
            /** @brief Apply an indexed native row operation. */
            edit(rows: WorksheetIntegratorRow[], index: number, operation: number): WorksheetIntegratorRow[];
        };

        readonly bindingPanel: BindingPanelState; /**< Rendered bindings and mutable state owned by the panel. */

        readonly bindingIntegratorPanel: { // Integrator row presentation.
            /** @brief Render native-prepared integrator rows. */
            renderIntegratorRows(rows: WorksheetIntegratorRow[]): void;
        };

        readonly bindingGoal: { // Goal-source lifecycle owned by the goal controller.
            /** @brief Clear the current native goal source. */
            clearGoalSeekRequest(): void;
        };

        readonly buffer: { // Native input text transfer.
            /** @brief Stage text in the selected native input buffer. */
            stageText(text: string, index?: number): number;
        };
    }
}
