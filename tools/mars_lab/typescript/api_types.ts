/**
 * @file api_types.ts
 * @brief Declare request resources, native continuation frames and service contracts.
 *
 * These type-only declarations merge into WorksheetRequests for api.ts and its
 * consumers. They describe browser resource ownership and the fields exchanged
 * with native C, while leaving mathematical payloads opaque where the browser
 * does not inspect them. Worksheet, transport and surface contracts supply shared
 * types. This file adds no runtime initialisation or script-order dependency.
 */
namespace WorksheetRequests {
    /** @brief Native continuation boundary; browser controllers supply inputs, not workflow policy. */
    export interface MarsApiFlowPort {
        /** @brief Continue an already selected native workflow with its opaque frame. */
        continue<T = unknown>(kind: number, frame: object): Promise<T>;
        /** @brief Ask native C to admit a workflow under its selected request lifetime. */
        run(kind: number, frame: object): Promise<unknown> | undefined;
    }

    /** @brief Deferred provider used after all classic-script definitions are available. */
    export type MarsApiCapabilityProvider = () => NativeServiceRegistry;

    /** @brief Narrow, lazily supplied dependencies of EditorPresentationController. */
    export interface MarsApiEditorPresentationControllerDependencies {
        readonly results: {
            readonly installLabPresentationData: typeof WorksheetResults.caches.install;
        };
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of EvaluationTransport. */
    export interface MarsApiEvaluationTransportDependencies {
        readonly workspace: {
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
            readonly currentIntegratorBoundRows: typeof WorksheetWorkspace.integrator.activeIntegratorBoundRows;
            readonly requestedValuePrecision: typeof WorksheetWorkspace.precision.requestedValuePrecision;
            readonly currentIntegratorRows: typeof WorksheetWorkspace.integrator.currentIntegratorRows;
            readonly integratorBoundsTextFromRows: typeof WorksheetWorkspace.integrator.forms.text;
            readonly refreshIntegratorForms: typeof WorksheetWorkspace.integrator.refreshIntegratorForms;
            readonly activeIntegratorRowPlan: typeof WorksheetWorkspace.integrator.activeIntegratorRowPlan;
            readonly requestedIntegratorIntervalCap: typeof WorksheetWorkspace.integrator.requestedIntegratorIntervalCap;
        };
        readonly bindings: {
            readonly currentExpressionText: typeof WorksheetBindings.editor.currentExpressionText;
            readonly renderIntegratorRows: typeof WorksheetBindings.integratorPanel.renderIntegratorRows;
        };
        readonly locations: {
            readonly currentDatetimeState: typeof WorksheetLocations.calendar.currentDatetimeState;
            readonly currentAlmanacState: typeof WorksheetLocations.calendar.currentAlmanacState;
        };
        readonly context: Pick<typeof WorksheetContext, 'worksheetSession' | 'expr'>;
        readonly native: Pick<typeof labDOM, 'call'>;
        readonly http: Pick<typeof labHttp, 'fetch'>;
        readonly wire: Pick<typeof labWire, 'encode'>;
        readonly flow: MarsApiFlowPort;
        readonly requests: typeof WorksheetRequests.labRequests;
    }

    /** @brief Narrow, lazily supplied dependencies of FunctionCardController. */
    export interface MarsApiFunctionCardControllerDependencies {
        readonly workspace: {
            readonly setActionRunning: typeof WorksheetWorkspace.presentation.setActionRunning;
        };
        readonly context: Pick<typeof WorksheetContext, 'functionRun'>;
        readonly native: Pick<typeof labDOM, 'call'>;
        readonly flow: MarsApiFlowPort;
        readonly requests: typeof WorksheetRequests.labRequests;
    }

    /** @brief Narrow, lazily supplied dependencies of PlotRequestController. */
    export interface MarsApiPlotRequestControllerDependencies {
        readonly workspace: {
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
            readonly variableNamesFromBindings: typeof WorksheetWorkspace.bindingSelection.variableNamesFromBindings;
            readonly renderDerivativeButtons: typeof WorksheetWorkspace.presentation.renderDerivativeButtons;
        };
        readonly bindings: {
            readonly visibleBindingValues: typeof WorksheetBindings.panel.visibleBindingValues;
            readonly expressionWithVisibleBindings: typeof WorksheetBindings.editor.expressionWithVisibleBindings;
        };
        readonly results: {
            readonly displayCalculusResult: typeof WorksheetResults.cards.displayCalculusResult;
            readonly setMatrixPrettyResult: typeof WorksheetResults.cards.setMatrixPrettyResult;
        };
        readonly context: Pick<typeof WorksheetContext, 'rightPaneTitle' | 'valueTitle'>;
        readonly presentationState: Pick<WorksheetPresentationState, 'lastMatrixScalarExpression'>;
        readonly bindingState: Pick<WorksheetBindings.WorksheetBindingPanel, 'currentVariables' | 'currentDifferentiable'>;
        readonly native: Pick<typeof labDOM, 'services' | 'call'>;
        readonly volume: Pick<typeof labSurfaceVolume, 'requesting' | 'cancel' | 'slider' | 'set' | 'schedule' | 'accept'>;
        readonly preview: Pick<typeof labSurfacePreview, 'prepare' | 'draw' | 'pinch' | 'rotate' | 'dispose' | 'clear' | 'snapshot'>;
        readonly document: Pick<typeof document, 'querySelector' | 'getElementById'>;
        readonly flow: MarsApiFlowPort;
        readonly requests: typeof WorksheetRequests.labRequests;
    }

    /** @brief Narrow, lazily supplied dependencies of SolverRenderController. */
    export interface MarsApiSolverRenderControllerDependencies {
        readonly workspace: {
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
        };
        readonly results: {
            readonly scheduleSolverTexFit: typeof WorksheetResults.solverFit.schedule;
        };
        readonly native: Pick<typeof labDOM, 'call'>;
        readonly flow: MarsApiFlowPort;
        readonly requests: typeof WorksheetRequests.labRequests;
    }

    /** @brief Narrow, lazily supplied dependencies of CalendarRequestController. */
    export interface MarsApiCalendarRequestControllerDependencies {
        readonly locations: {
            readonly refreshDatetimeJurisdictionLocation: typeof WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation;
        };
        readonly results: {
            readonly bindAlmanacTotalityActions: typeof WorksheetResults.calendarResults.bindAlmanacTotalityActions;
        };
        readonly context: Pick<typeof WorksheetContext, 'controlToken'>;
        readonly native: Pick<typeof labDOM, 'call'>;
        readonly http: Pick<typeof labHttp, 'fetch'>;
        readonly application: Pick<typeof labApplication, 'configuration'>;
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of EvaluationController. */
    export interface MarsApiEvaluationControllerDependencies {
        readonly workspace: {
            readonly workspaceModeId: typeof WorksheetWorkspace.mode.workspaceModeId;
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
            readonly pushExpressionHistory: typeof WorksheetWorkspace.history.pushExpressionHistory;
            readonly showResults: typeof WorksheetWorkspace.presentation.showResults;
            readonly updateHistoryButtons: typeof WorksheetWorkspace.presentation.updateHistoryButtons;
            readonly requestedValuePrecision: typeof WorksheetWorkspace.precision.requestedValuePrecision;
        };
        readonly bindings: {
            readonly currentExpressionText: typeof WorksheetBindings.editor.currentExpressionText;
            readonly visibleBindingValues: typeof WorksheetBindings.panel.visibleBindingValues;
            readonly commitVisibleBindingInputs: typeof WorksheetBindings.controller.commitVisibleBindingInputs;
            readonly expressionWithVisibleBindings: typeof WorksheetBindings.editor.expressionWithVisibleBindings;
        };
        readonly locations: {
            readonly currentDatetimeState: typeof WorksheetLocations.calendar.currentDatetimeState;
            readonly datetimeSummaryText: typeof WorksheetLocations.calendar.datetimeSummaryText;
            readonly almanacSummaryText: typeof WorksheetLocations.calendar.almanacSummaryText;
            readonly currentAlmanacState: typeof WorksheetLocations.calendar.currentAlmanacState;
        };
        readonly persistence: {
            readonly historyStateForMode: typeof WorksheetPersistence.history.snapshot;
            readonly previousModeStateForHistory: typeof WorksheetPersistence.history.previous;
            readonly commitModeState: typeof WorksheetPersistence.history.commit;
        };
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of GoalSeekController. */
    export interface MarsApiGoalSeekControllerDependencies {
        readonly workspace: {
            readonly workspaceModeId: typeof WorksheetWorkspace.mode.workspaceModeId;
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
        };
        readonly bindings: {
            readonly currentExpressionText: typeof WorksheetBindings.editor.currentExpressionText;
            readonly goalSeekExpressionAndStarts: typeof WorksheetBindings.goal.goalSeekExpressionAndStarts;
            readonly expressionBodyForEditor: typeof WorksheetBindings.editor.expressionBodyForEditor;
            readonly expressionWithBindings: typeof WorksheetBindings.editor.expressionWithBindings;
            readonly expression_text_or_empty: typeof WorksheetBindings.expression_text_or_empty;
        };
        readonly native: Pick<typeof labDOM, 'call'>;
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of NativeFlowRuntime. */
    export interface MarsApiNativeFlowRuntimeDependencies {
        readonly workspace: {
            readonly currentMode: typeof WorksheetWorkspace.mode.currentMode;
        };
        readonly results: {
            readonly labEvaluationView: typeof WorksheetResults.labEvaluationView;
        };
        readonly native: Pick<typeof labDOM, 'flow' | 'call' | 'services'>;
        readonly requests: typeof WorksheetRequests.labRequests;
    }

    /** @brief Browser resources attached to one native channel and token. */
    export interface MarsApiRequest {
        channel: number;
        token: number;
        controller: AbortController;
        timer: number | undefined;
        button: HTMLButtonElement | null;
        disableButton: boolean;
    }

    /** @brief Native admission inputs and optional browser button ownership for a request. */
    export interface MarsApiOptions {
        input?: boolean;
        variable?: boolean;
        scalar?: boolean;
        parent?: number;
        button?: HTMLButtonElement | null;
        disableButton?: boolean;
    }

    /** @brief Explicit matrix payload overrides and optional existing request ownership. */
    export interface MarsApiMatrixOptions extends MarsApiOptions {
        request?: MarsApiRequest | null;
        operand?: unknown;
        matrixText?: string;
        operation?: string;
        skipSave?: boolean;
    }

    /** @brief Opaque native response data with the acceptance fields consumed by transport. */
    export interface MarsApiData extends Partial<WorksheetEvaluationData> {
        ok?: boolean;
        partial_error?: unknown;
        [key: string]: unknown;
    }

    /** @brief Decoded native payload paired with its original transport response. */
    export interface MarsApiPostResult {
        response: MarsTransportResponse;
        data: MarsApiData;
    }

    /** @brief Mutable native transport continuation before response decoding completes. */
    export interface MarsApiPostFrame {
        request: MarsApiRequest | null;
        payload: unknown;
        fallback?: string;
        signal?: AbortSignal;
        response?: MarsTransportResponse;
        data?: MarsApiData;
        result?: MarsApiPostResult;
    }

    /** @brief Transport continuation after both response and payload are available. */
    export interface MarsApiDecodedPostFrame extends MarsApiPostFrame {
        response: MarsTransportResponse;
        data: MarsApiData;
    }

    /** @brief Owned callback invocation and its native-controlled completion value. */
    export interface MarsApiRunFrame<T> {
        operation: string;
        mode: string;
        callback: (request: MarsApiRequest) => T | Promise<T>;
        options: MarsApiOptions;
        request?: MarsApiRequest;
        result?: T;
    }

    /** @brief Editor promise and the native context that owns its cache entry. */
    export interface MarsApiEditorPending {
        context: number;
        promise: Promise<WorksheetEditorMetadata> | null;
    }

    /** @brief Native editor-analysis request without browser-side expression rewriting. */
    export interface MarsApiEditorPayload {
        action: 'editor';
        operation: 'analyse';
        text: string;
    }
    /** Fields captured by lab_solver_view_prepare before a browser request is attached. */
    export interface MarsApiSolverSnapshot {
        node: HTMLElement;
        compactSvg: string;
        wrappedTex: string;
        parentToken: string;
        restoredOwner: unknown;
        mode: string;
        operation: string;
        options: MarsApiOptions;
        payload: {tex: string};
        endpoint: string;
    }

    /** @brief Prepared solver snapshot with its attached request and freshness guard. */
    export interface MarsApiSolverPending extends MarsApiSolverSnapshot {
        request: MarsApiRequest;
        current?: () => boolean;
    }

    /** @brief Native gesture state carrying browser request and preview coordination fields. */
    export interface MarsApiGesture extends MarsSurfaceGesture {
        revision?: number;
        pendingToken?: number;
        rangeWheel?: boolean;
        pan?: boolean;
        pinch?: boolean;
        active?: boolean;
        wheel?: boolean;
        panProjection?: string;
    }

    /** @brief Arguments retained when native gesture completion requests another redraw. */
    export type MarsApiRedrawArguments = [state: MarsApiGesture, variable: string, preview: boolean, refine?: boolean];

    /** @brief Discriminated native fetch modes; only matrix requests carry matrix options. */
    export type MarsApiFetchFrame =
        | {mode: 0; options: MarsApiMatrixOptions; request: MarsApiRequest | null}
        | {mode: 1 | 2 | 3 | 4; request: MarsApiRequest | null};

    /** @brief Native admission plan selecting request ownership and UI recovery behaviour. */
    export interface MarsApiFlowPlan {
        operation?: string;
        ui?: boolean;
        mode: string;
        options?: MarsApiOptions;
    }

    /** @brief Captured worksheet inputs consumed by native evaluation services. */
    export interface MarsApiEvaluationContext {
        enteredBindings: WorksheetBinding[];
        state: WorksheetCalendarState;
        text: string;
        editorText: string;
        entered?: unknown;
    }

    /** @brief Initial mode and options supplied to the native evaluation continuation. */
    export interface MarsApiEvaluationFrame {
        mode: number;
        options: WorksheetEvaluationOptions;
        request?: MarsApiRequest;
    }

    /** @brief Opaque result and captured context supplied to native result installation. */
    export interface MarsApiInstallFrame {
        mode: number;
        data: unknown;
        context: Partial<MarsApiEvaluationContext>;
        outcome: number;
        request?: MarsApiRequest | null;
    }

    /** @brief Evaluation options extended with an explicit goal binding-commit request. */
    export interface MarsApiGoalOptions extends WorksheetEvaluationOptions {
        commitBindings?: boolean;
    }

    /** @brief Native goal-seeking inputs and browser evaluation options. */
    export interface MarsApiGoalFrame {
        sourceText: string;
        target: string;
        start: Record<string, unknown>;
        options: MarsApiGoalOptions;
    }

    /** @brief Initial manual or automatic calculus request passed to native C. */
    export interface MarsApiCalculusFrame {
        /** @brief Internal representation retained when resampling an existing complex curve. */
        plotKind?: 'complex_curve';
        wrt?: string;
        action: string;
        actionButton?: HTMLButtonElement | null;
        preview?: boolean;
        live?: boolean;
        automatic?: boolean;
        request?: MarsApiRequest;
    }

    /** @brief Atomic browser capabilities for native transport and request continuations. */
    export interface MarsApiTransportServices {

        /** @brief Reject an obsolete or timed-out request before beginning transport work. */
        assertCurrent(request: MarsApiRequest | null): void;

        /** @brief Synchronise the worksheet mode and report whether the native request token remains live. */
        current(request: MarsApiRequest | null): boolean;

        /** @brief Finish native request ownership and release browser resources that are no longer live. */
        finish(request: MarsApiRequest | null): void;

        /** @brief Ask native C to admit the captured operation and acquire its browser resources. */
        begin(frame: MarsApiRunFrame<unknown>): MarsApiRequest | null;

        /** @brief Invoke the captured operation with its admitted request; reject missing ownership. */
        callback(frame: MarsApiRunFrame<unknown>): unknown;

        /** @brief Read the native-selected endpoint for the admitted request. */
        endpoint(request: MarsApiRequest): string;

        /** @brief Post the encoded payload using the request's abort signal, or the supplied fallback signal. */
        fetch(endpoint: string, frame: MarsApiPostFrame): Promise<MarsTransportResponse>;

        /** @brief Decode the response body through the transport's bounded native codec. */
        decode(response: MarsTransportResponse): Promise<unknown>;

        /** @brief Install native presentation metadata from the decoded reply. */
        presentation(frame: {data: MarsApiData}): unknown;

        /** @brief Install native function-syntax metadata from the decoded reply. */
        syntax(frame: {data: MarsApiData}): unknown;

        /** @brief Pair the decoded native data with its original transport response. */
        result(frame: MarsApiDecodedPostFrame): MarsApiPostResult;

        /** @brief Reject a native transport plan that does not select a valid endpoint. */
        invalidEndpoint(): never;

        /** @brief Propagate the captured exception without replacing its identity. */
        rethrow(frame: {exception: unknown}): never;

        /** @brief Present the supplied diagnostic in the rendered-result card. */
        renderError(error: string): void;

        /** @brief Reset the rendered-result precision control after request failure. */
        resetDigits(): void;

        /** @brief Clear the result details selected by the native recovery options. */
        clearResults(options: Parameters<typeof WorksheetResults.reset.clearResultDetails>[0]): void;

        /** @brief Update the worksheet status without interpreting the native result. */
        status(text: string): void;
    }

    /** @brief Capabilities exposing pending editor resources to native cache selection. */
    export interface MarsApiEditorServices {

        /** @brief Expose this controller's pending editor promises for native cache selection. */
        editorRequests(): Map<string, MarsApiEditorPending>;

        /** @brief Start editor preparation and remove its cache entry only while the same pending record owns it. */
        editorPromise(source: string, pending: MarsApiEditorPending, payload: MarsApiEditorPayload): Promise<WorksheetEditorMetadata>;
    }

    /** @brief Native-selected plot action supported by the browser slice queue. */
    export interface MarsApiSliceServices {

        /** @brief Request a live plot slice with the native-selected preview quality. */
        takePlot(wrt: string, preview: boolean): Promise<unknown>;
    }

    /** @brief Optional abort signal borrowed by a native form-preparation request. */
    export interface MarsApiFormsOptions { signal?: AbortSignal; }

    /** @brief Opaque form payload and its browser transport options. */
    export interface MarsApiFormsFrame { payload: unknown; options: MarsApiFormsOptions; }

    /** @brief Distinct inputs for presentation fetching and cached editor preparation. */
    export type MarsApiPresentationFrame =
        | {payload: unknown; request: MarsApiRequest | null}
        | {editor: true; text: string};

    /** @brief Captured integral source, rows and native context used for stale-work checks. */
    export interface MarsApiIntegratorSnapshot {
        text: string;
        rows: WorksheetIntegratorRow[];
        context: number;
        rowsText: string;
    }

    /** @brief Existing request ownership for the native integrator continuation. */
    export interface MarsApiIntegratorFrame { request: MarsApiRequest | null; }
    /** Atomic capabilities available to request continuations selected by native C. */
    export type MarsApiRequestFlowServices = MarsApiEditorServices & {

        /** @brief Admit an operation under native request policy and acquire its browser resources. */
        requestBegin: typeof WorksheetRequests.labRequests.begin;

        /** @brief Admit an operation using the worksheet mode at the time of the call. */
        requestBeginCurrent(operation: string, options: MarsApiOptions): MarsApiRequest | null;

        /** @brief Finish the supplied native request and reap its obsolete browser resources. */
        requestFinish: typeof WorksheetRequests.labRequests.finish;

        /** @brief Cancel the named operation's native channel and release its obsolete resources. */
        requestCancel: typeof WorksheetRequests.labRequests.cancel;

        /** @brief Read the latest unsigned main-request token for freshness checks. */
        requestLatestMain: typeof WorksheetRequests.labRequests.latestMain;

        /** @brief Run the selected continuation stage with native-owned admission and UI error recovery. */
        runRequestFlow(kind: number, frame: object, stage: number, operation: string, mode: WorksheetMode,
                       options: MarsApiOptions): Promise<unknown>;

        /** @brief Post an encoded native payload to the supplied endpoint with optional cancellation. */
        rawPost(endpoint: string, payload: unknown, signal?: AbortSignal): Promise<MarsTransportResponse>;

        /** @brief Decode the response body without reinterpreting its native payload. */
        responseData(response: MarsTransportResponse): Promise<unknown>;

        /** @brief Install native presentation metadata into the result caches. */
        installPresentation: typeof WorksheetResults.caches.install;

        /** @brief Return a value unchanged so the native continuation can await it. */
        awaitValue(value: unknown): unknown;

        /** @brief Throw an AbortError carrying the native-selected diagnostic. */
        abort(message: string): never;

        /** @brief Fetch uncached mobile-access information with the configured control token. */
        mobileFetch(): Promise<MarsTransportResponse>;

        /** @brief Cancel function-card work and clear its presentation and busy indicator. */
        clearFunctionRun: typeof WorksheetRequests.functionCard.clearFunctionRun;

        /** @brief Ask native C to construct a payload using current precision and captured inputs. */
        buildPayload(kind: number, inputs: unknown[], request: MarsApiRequest | null): unknown;

        /** @brief Report response acceptance and timeout state to the native function-card presenter. */
        functionComplete(request: MarsApiRequest, outcome: number, data: unknown): unknown;

        /** @brief Report a function-card exception while preserving ordinary message-getter behaviour. */
        functionException(request: MarsApiRequest, error: unknown): unknown;

        /** @brief Capture the integral source, bounds and native context for freshness checks. */
        integratorSnapshot(): MarsApiIntegratorSnapshot;

        /** @brief Read the authored integral source, falling back to the visible editor. */
        integratorText(): string;

        /** @brief Serialise the current integral-bound rows through the workspace adapter. */
        integratorRowsText(): string;

        /** @brief Refresh native integral metadata before preparing bound rows. */
        refreshIntegratorForms: typeof WorksheetWorkspace.integrator.refreshIntegratorForms;

        /** @brief Ask the workspace adapter for the active native integral-row plan. */
        activeIntegratorRows: typeof WorksheetWorkspace.integrator.activeIntegratorRowPlan;

        /** @brief Project the supplied integral-bound rows into their editor controls. */
        renderIntegratorRows: typeof WorksheetBindings.integratorPanel.renderIntegratorRows;

        /** @brief Ask native C to build an integral payload with the current precision and interval cap. */
        integratorPayload(text: string, bounds: WorksheetIntegratorRow[], request: MarsApiRequest | null): unknown;

        /** @brief Capture mode-specific inputs and let native C construct any mathematical payload. */
        fetchPayload(frame: MarsApiFetchFrame): unknown;

        /** @brief Ask native C to assemble the configured land-totality request. */
        landPayload(data: unknown, cells: unknown): unknown;

        /** @brief Attach almanac totality actions to the supplied result element. */
        bindAlmanacTotalityActions(node: HTMLElement): void;

        /** @brief Capture the active worksheet mode and retained scalar matrix expression. */
        calculusInitial(): {mode: WorksheetMode; scalar: string};

        /** @brief Snapshot visible bindings, variables and differentiability before calculus work. */
        calculusCapture(): {bindings: WorksheetBinding[]; variables: string[]; differentiable: boolean};

        /** @brief Read the retained scalar expression from the latest matrix result. */
        matrixScalar(): string;

        /** @brief Ask the binding editor to assemble authored values without client-side algebra. */
        expressionWithVisibleBindings: typeof WorksheetBindings.editor.expressionWithVisibleBindings;

        /** @brief Set and return the native-selected result-pane title. */
        calculusTitle(text: string): string;

        /** @brief Install native plot output while retaining the appropriate gesture projection or camera. */
        displayPlot(result: Record<string, unknown>): unknown;

        /** @brief Present a plot failure through the native layout adapter. */
        plotError(error: unknown): unknown;

        /** @brief Start background plotting tied to the parent token without extending its busy lifetime. */
        automaticPlot(parent: MarsApiRequest): void;

        /** @brief Set and return the native-selected numerical-value card title. */
        valueTitle(text: string): string;

        /** @brief Request native presentation metadata without interpreting the payload. */
        requestPresentation: typeof WorksheetRequests.presentation.requestLabPresentation;

        /** @brief Fetch the native calculus result using the captured request and preview quality. */
        fetchCalculus(text: string, wrt: string, action: string, request: MarsApiRequest | null,
                      preview: boolean): Promise<MarsApiPostResult>;

        /** @brief Fetch a matrix result using explicit payload overrides and request ownership. */
        fetchMatrixOptions(options: MarsApiMatrixOptions): Promise<MarsApiPostResult>;

        /** @brief Project the native calculus result into its result cards. */
        displayCalculusResult: typeof WorksheetResults.cards.displayCalculusResult;

        /** @brief Install the native matrix rendering without rewriting its mathematics. */
        setMatrixPrettyResult: typeof WorksheetResults.cards.setMatrixPrettyResult;

        /** @brief Replace the retained calculus-variable list and return the installed list. */
        calculusVariables(variables: string[]): string[];

        /** @brief Derive the displayed variable list from the supplied native binding metadata. */
        calculusBindingVariables(bindings: WorksheetBinding[] | null): string[];

        /** @brief Store and return whether native metadata permits differentiation. */
        calculusDifferentiable(enabled: boolean): boolean;

        /** @brief Refresh calculus controls using the retained variable list. */
        calculusButtons(): void;

        /** @brief Ask native C to prepare a responsive solver snapshot if its owner is still current. */
        solverPrepare(): MarsApiSolverSnapshot | null;

        /** @brief Attach request ownership and a native freshness guard to a solver snapshot. */
        solverPending(pending: MarsApiSolverSnapshot, request: MarsApiRequest): void;

        /** @brief Ask native C to publish solver output only for the current snapshot and request. */
        solverPublish(pending: MarsApiSolverPending, response: unknown, data: unknown, failed: boolean): unknown;

        /** @brief Release the pending solver snapshot only when it is still the same owned object. */
        solverRelease(pending: MarsApiSolverPending): void;
    };

}
