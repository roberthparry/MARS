/**
 * @file results_types.d.ts
 * @brief Browser capabilities borrowed by result-card controllers.
 *
 * These contracts describe the small part of each owner used by results.ts.
 * Native results remain opaque until the consuming adapter selects a documented
 * response shape. No mathematical processing or runtime state belongs here.
 */
declare namespace WorksheetResults {
    /** @brief Scoped native projection; each caller names the result shape it consumes. */
    interface ResultNative {
        /** @brief Invoke a native entry point with borrowed browser values. */
        call(entry: string, ...args: unknown[]): unknown;
    }

    /** @brief Native projection plus deferred browser-service dispatch. */
    interface ResultNativeServices extends ResultNative {
        /** @brief Dispatch native-selected services outside the handle scope. */
        services(plan: NativeServicePlan, registry: NativeServiceRegistry): unknown;
    }

    /** @brief Native asynchronous result workflow. */
    interface ResultFlow {
        /** @brief Advance the selected native workflow with its browser-owned frame. */
        continue(kind: number, frame: object): Promise<unknown>;
    }

    /** @brief Cancel result-specific function execution. */
    interface ResultFunctionRun {
        /** @brief Clear the current function run without disposing the request owner. */
        clearFunctionRun(): void;
    }

    /** @brief Read the active worksheet mode when an action runs. */
    interface ResultMode {
        /** @brief Return the current native worksheet mode. */
        currentMode(): WorksheetMode;
    }

    /** @brief Native card presentation fields; evaluation metadata may be absent. */
    interface ResultData {
        editor?: WorksheetEditorMetadata;
        rows?: WorksheetIntegratorRow[];
        binding_values?: WorksheetBinding[];
        evaluation_ready?: boolean;
        [field: string]: unknown;
    }

    /** @brief Reusable binding state shared with the workspace presenter. */
    interface ResultBindingState { resultInputBindings: WorksheetBinding[]; }

    /** @brief Native TeX and reusable bindings updated by card installation. */
    interface ResultCardState extends ResultBindingState { lastTex: string; }

    /** @brief Presentation fields cleared when a result is reset. */
    interface ResultResetState extends ResultCardState { lastDerivativeExpression: string; }

    /** @brief Native reset values accompanied by deferred browser actions. */
    interface ResultResetPlan extends WorksheetResultValues, NativeServicePlan {}

    /** @brief Native calendar sections and their exact clipboard text. */
    interface ResultCalendarSections { sections: unknown; text: string; }

    /** @brief Editor and function cards targeted by native text presentation. */
    interface ResultCardElements {
        functionStyle: HTMLElement;
        functionMore: WorksheetCopyButton;
        parsed: HTMLElement;
        parsedMore: WorksheetCopyButton;
    }

    /** @brief Browser clipboard write capability. */
    interface ResultClipboard {
        /** @brief Write text through the browser's secure clipboard API. */
        writeText(text: string): Promise<void>;
    }

    /** @brief DOM operations required by the fallback clipboard selection. */
    interface ResultClipboardDocument {
        body: HTMLElement;
        /** @brief Allocate the temporary text selection. */
        createElement(tag: 'textarea'): HTMLTextAreaElement;

        /** @brief Ask the browser to copy the active selection. */
        execCommand(command: string): boolean;
    }

    /** @brief Clipboard sources, browser operations and feedback timer ownership. */
    interface ClipboardDependencies {
        /** @brief Resolve the scoped native adapter when the operation runs. */
        native(): ResultNative;

        /** @brief Borrow DOM operations for a temporary clipboard selection. */
        document(): ResultClipboardDocument;

        /** @brief Return secure clipboard access when the browser provides it. */
        clipboard(): ResultClipboard | undefined;

        /** @brief Report whether the page is a secure browser context. */
        secure(): boolean;

        /** @brief Schedule one callback and return its cancellable timer ID. */
        schedule(callback: () => void, delay: number): number;

        /** @brief Cancel the supplied owned timer. */
        cancel(timer: number): void;

        /** @brief Export the current surface as SVG, or return null. */
        surface(): string | null;

        /** @brief Read the exact native TeX retained with the result. */
        lastTeX(): string;
    }

    /** @brief Store the native input text and optionally supplied bindings for reuse. */
    interface ResultInput {
        /** @brief Retain text and a native snapshot of the supplied bindings. */
        setResultInputText(text: string, bindings?: WorksheetBinding[] | null): void;
    }

    /** @brief Capabilities consumed while installing result cards. */
    interface ResultCardPresenterDependencies {
        /** @brief Resolve the current browser controls without caching global state. */
        context(): ResultCardElements;
        readonly presentationState: ResultCardState;

        /** @brief Resolve the native continuation runner. */
        flow(): ResultFlow;

        /** @brief Resolve cancellation of the current function run. */
        functionRun(): ResultFunctionRun;

        /** @brief Resolve the native input-text store. */
        reuse(): ResultInput;

        /** @brief Resolve the scoped native adapter when the operation runs. */
        native(): ResultNative;
        caches: WorksheetPresentationCaches;
    }

    /** @brief Capabilities for transferring a result back into the editor. */
    interface ResultReuseControllerDependencies {
        readonly presentationState: ResultBindingState;

        /** @brief Resolve the native continuation runner. */
        flow(): ResultFlow;

        /** @brief Resolve the current worksheet mode. */
        mode(): ResultMode;

        /** @brief Resolve the scoped native adapter when the operation runs. */
        native(): ResultNative;
    }

    /** @brief Controls used by copy-input actions. */
    interface ResultActionElements { inputCopy: WorksheetCopyButton; }

    /** @brief Native actions and browser status-timer operations. */
    interface ResultActionControllerDependencies {
        /** @brief Resolve the current browser controls without caching global state. */
        context(): ResultActionElements;

        /** @brief Resolve the native continuation runner. */
        flow(): ResultFlow;

        /** @brief Replace the visible workspace status text. */
        status(text: string): void;

        /** @brief Schedule one callback and return its cancellable timer ID. */
        schedule(callback: () => void, delay: number): number;

        /** @brief Cancel the supplied owned timer. */
        cancel(timer: number): void;
    }

    /** @brief Derivative controls refreshed after native result reset. */
    interface ResultDerivativeControls {
        /** @brief Refresh derivative buttons for the supplied native variable names. */
        renderDerivativeButtons(variables: string[]): void;
    }

    /** @brief Binding-panel state updated by native result reset. */
    interface ResultBindings {
        currentVariables: string[];
        currentDifferentiable: boolean;
        /** @brief Clear visible variable-value controls. */
        clearVariableValues(): void;
    }

    /** @brief Local-date content controlled by the native calendar presenter. */
    interface ResultLocalDate {
        /** @brief Install local-date text and optional native section metadata. */
        setDatetimeLocalText(text: string, sections?: unknown): void;
    }

    /** @brief Capabilities needed to clear cards and their associated browser state. */
    interface ResultResetControllerDependencies {
        readonly presentationState: ResultResetState;

        /** @brief Resolve cancellation of the current function run. */
        functionRun(): ResultFunctionRun;

        /** @brief Resolve the current worksheet mode. */
        mode(): ResultMode;

        /** @brief Resolve derivative-control presentation. */
        presentation(): ResultDerivativeControls;

        /** @brief Borrow the live binding-panel state. */
        bindings(): ResultBindings;

        /** @brief Resolve the local-date presenter. */
        calendarResults(): ResultLocalDate;

        /** @brief Resolve the scoped native adapter when the operation runs. */
        native(): ResultNativeServices;
        caches: WorksheetPresentationCaches;
    }

    /** @brief Reset options passed unchanged to native policy. */
    interface WorksheetResultResetOptions { keepBindings?: boolean; }

    /** @brief Live calendar state read by native almanac presentation. */
    interface ResultAlmanacState {
        readonly visibility: string;
        readonly worksheet: unknown;
    }

    /** @brief Calendar controls and the only session field modified by this presenter. */
    interface ResultCalendarContext {
        worksheetSession: {calendar: {almanacVisibilityMode: string}};
        datetimeLocalBody: HTMLElement;
        datetimeLocal: HTMLElement;
    }

    /** @brief Native calendar presentation and deferred location services. */
    interface CalendarResultPresenterDependencies {
        /** @brief Resolve the current browser controls without caching global state. */
        context(): ResultCalendarContext;

        /** @brief Resolve the scoped native adapter when the operation runs. */
        native(): ResultNativeServices;

        /** @brief Resolve the native-selected calendar service registry. */
        locationServices(): NativeServiceRegistry;
        almanacState: ResultAlmanacState;
    }
}
