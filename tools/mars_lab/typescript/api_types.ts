/**
 * @file api_types.ts
 * @brief Declare request resources, native continuation frames and service contracts.
 *
 * These type-only declarations merge into WorksheetRequests for api.ts and its
 * consumers. They describe browser resource ownership and the fields exchanged
 * with native C, while leaving mathematical payloads opaque where the browser
 * does not inspect them. Worksheet, transport and surface contracts supply shared
 * types. Controllers allocate input frames; native continuations add staged outputs and select
 * service calls, awaiting only stages marked asynchronous. Optional output fields are not promises
 * of availability before their producing stage. Frames contain data and borrowed browser resources,
 * never methods that duplicate native workflow policy. This file adds no runtime initialisation
 * or script-order dependency.
 */
namespace WorksheetRequests {
    // Browser ports shared by controllers; owners are supplied lazily by the composition root.

    /** @brief Opaque named native values, without a browser-side mathematical schema. */
    export interface MarsApiRecord { [key: string]: unknown; }

    /** @brief Authored starting values accepted by native goal preparation. */
    export interface MarsApiStartValues { [key: string]: string; }

    /** @brief Invoke a native entry point; callers cast only its documented result boundary. */
    export interface MarsApiNativePort {
        /** @brief Pass host values to the native DOM bridge. */
        call(entry: string, ...args: unknown[]): unknown;
    }

    /** @brief Native DOM projection and synchronous service-plan execution. */
    export interface MarsApiProjectionPort extends MarsApiNativePort {
        /** @brief Dispatch a native plan through the supplied own-property service table. */
        services(plan: NativeServicePlan, registry: NativeServiceRegistry | null): unknown;
    }

    /** @brief Native continuations with scoped handles and abort-aware promise dispatch. */
    export interface MarsApiContinuationPort extends MarsApiProjectionPort {
        /** @brief Advance a mutable frame until native completion, awaiting only native-selected stages. */
        flow(kind: number, frame: object, view: unknown, registry: NativeServiceRegistry,
             signal?: AbortSignal): Promise<unknown>;
    }

    /** @brief HTTP transport returning responses with native binary decoding. */
    export interface MarsApiHttpPort {
        /** @brief Fetch a resource with ordinary browser request options. */
        fetch(url: RequestInfo | URL, options?: RequestInit): Promise<MarsTransportResponse>;
    }

    /** @brief Admitted request action; native cleanup preserves its resolved result type. */
    export interface MarsApiRequestAction<T = unknown> { (request: MarsApiRequest): T | Promise<T>; }

    /** @brief Result-detail reset policy supplied by native request recovery. */
    export interface MarsApiResultResetOptions {
        /** @brief Retain visible binding controls while clearing result details. */
        keepBindings?: boolean;
    }

    /** @brief Response acceptance flag used by native request classification. */
    export interface MarsApiResponseStatus { readonly ok: boolean; }

    /** @brief Request lifetimes and transport shared by browser workflow controllers. */
    export interface MarsApiRequestPort {
        /** @brief Admit an operation and acquire its browser resources. */
        begin(operation: string, mode?: string, options?: MarsApiOptions): MarsApiRequest | null;

        /** @brief Check whether request ownership remains current. */
        current(request: MarsApiRequest | null | undefined): boolean;

        /** @brief Finish ownership and release obsolete resources. */
        finish(request: MarsApiRequest | null | undefined): void;

        /** @brief Cancel the native channel associated with an operation. */
        cancel(operation: string): void;

        /** @brief Read the latest main-request token. */
        latestMain(): number;

        /** @brief Read the current native worksheet context. */
        context(): number;

        /** @brief Report expiry of the request deadline. */
        timedOut(request: MarsApiRequest): boolean;

        /** @brief Classify response acceptance through native request policy. */
        outcome(request: MarsApiRequest | null, response: MarsApiResponseStatus | null,
                data: MarsApiData | null, result?: boolean): number;

        /** @brief Validate input availability against current request ownership. */
        input(request: MarsApiRequest, text: unknown, variable?: boolean): boolean;

        /** @brief Post a native payload under the supplied request or fallback abort signal. */
        post(request: MarsApiRequest | null, payload: unknown, fallback: string,
             signal?: AbortSignal): Promise<MarsApiPostResult>;

        /** @brief Execute an admitted action, preserving its result type; rejected admission returns undefined. */
        run<T>(operation: string, mode: string, callback: MarsApiRequestAction<T>, options?: MarsApiOptions): Promise<T | undefined>;

        /** @brief Execute an admitted action with native UI error recovery. */
        runUI<T>(operation: string, mode: string, callback: MarsApiRequestAction<T>, options?: MarsApiOptions): Promise<T | undefined>;

        /** @brief Synchronise native mode ownership and release obsolete requests. */
        modeChanged(mode: string): void;
    }

    /** @brief Native continuation boundary; browser controllers supply inputs, not workflow policy. */
    export interface MarsApiFlowPort {
        /** @brief Continue an already selected native workflow with its opaque frame. */
        continue(kind: number, frame: object): Promise<unknown>;

        /** @brief Ask native C to admit a workflow under its selected request lifetime. */
        run(kind: number, frame: object): Promise<unknown> | undefined;
    }

    /** @brief Deferred provider used after all classic-script definitions are available. */
    export interface MarsApiCapabilityProvider { (): NativeServiceRegistry; }

    /** @brief Narrow, lazily supplied dependencies of EditorPresentationController. */
    export interface MarsApiEditorPresentationControllerDependencies {
        readonly results: {
            /** @brief Install native presentation metadata in the result caches. */
            installLabPresentationData(data: unknown): void;
        };
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of EvaluationTransport. */
    export interface MarsApiEvaluationTransportDependencies {
        readonly workspace: {
            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;

            /** @brief Read bound rows selected by native integrator policy. */
            currentIntegratorBoundRows(rows?: WorksheetIntegratorRow[], expressionText?: string): WorksheetIntegratorRow[];

            /** @brief Read the requested numerical precision. */
            requestedValuePrecision(): number;

            /** @brief Read structured integrator controls. */
            currentIntegratorRows(): WorksheetIntegratorRow[];

            /** @brief Format integrator rows through the native forms adapter. */
            integratorBoundsTextFromRows(rows: WorksheetIntegratorRow[]): string;

            /** @brief Refresh native form metadata and return accepted rows. */
            refreshIntegratorForms(rows?: WorksheetIntegratorRow[], expression?: string): Promise<WorksheetIntegratorRow[] | null | undefined>;

            /** @brief Obtain the native active-row plan for captured inputs. */
            activeIntegratorRowPlan(rows?: WorksheetIntegratorRow[], expressionText?: string): WorksheetRowPlan;

            /** @brief Read the native-validated integration budget. */
            requestedIntegratorIntervalCap(): number;
        };
        readonly bindings: {
            /** @brief Read the current authored expression. */
            currentExpressionText(): string;

            /** @brief Project structured rows into integrator controls. */
            renderIntegratorRows(rows: WorksheetIntegratorRow[]): void;
        };
        readonly locations: {
            /** @brief Capture native-normalised DateTime controls. */
            currentDatetimeState(): WorksheetCalendarState;

            /** @brief Capture native-normalised almanac controls. */
            currentAlmanacState(): WorksheetCalendarState;
        };
        readonly context: {worksheetSession: {editor: {lastExpressionUpdatedAt: number}}; expr: HTMLTextAreaElement};
        readonly native: MarsApiNativePort;
        readonly http: MarsApiHttpPort;
        readonly wire: {
            /** @brief Encode an opaque payload with the native wire codec. */
            encode(value: unknown): Uint8Array<ArrayBuffer>;
        };
        readonly flow: MarsApiFlowPort;
        readonly requests: MarsApiRequestPort;
    }

    /** @brief Narrow, lazily supplied dependencies of FunctionCardController. */
    export interface MarsApiFunctionCardControllerDependencies {
        readonly workspace: {
            /** @brief Set the action button's running indicator. */
            setActionRunning(button: WorksheetCopyButton, running: boolean): void;
        };
        readonly context: {functionRun: WorksheetCopyButton};
        readonly native: MarsApiNativePort;
        readonly flow: MarsApiFlowPort;
        readonly requests: MarsApiRequestPort;
    }

    /** @brief Narrow, lazily supplied dependencies of PlotRequestController. */
    export interface MarsApiPlotRequestControllerDependencies {
        readonly workspace: {
            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;

            /** @brief Read variable names from native binding metadata. */
            variableNamesFromBindings(bindings: WorksheetBinding[]): string[];

            /** @brief Refresh derivative controls for the supplied variables. */
            renderDerivativeButtons(variables: string[]): void;
        };
        readonly bindings: {
            /** @brief Capture authored values from visible binding inputs. */
            visibleBindingValues(): WorksheetBinding[];

            /** @brief Assemble authored expression and visible bindings through native policy. */
            expressionWithVisibleBindings(sourceExpression: string, visibleBindings: WorksheetBinding[]): Promise<string>;
        };
        readonly results: {
            /** @brief Install native calculus rendering and reusable bindings. */
            displayCalculusResult(result: MarsApiData): void;

            /** @brief Present native matrix text in the supplied result card. */
            setMatrixPrettyResult(resultText: string, prettyText: string, element?: HTMLElement, moreButton?: WorksheetCopyButton): Promise<unknown>;
        };
        readonly context: {rightPaneTitle: HTMLElement; valueTitle: HTMLElement};
        readonly presentationState: {lastMatrixScalarExpression: string};
        readonly bindingState: {currentVariables: string[]; currentDifferentiable: boolean};
        readonly native: MarsApiProjectionPort;
        readonly volume: {
            readonly requesting: boolean;

            /** @brief Cancel pending surface refinement. */
            cancel(manual?: boolean): void;

            /** @brief Handle native surface slider hold or release. */
            slider(action: number): void;

            /** @brief Apply native surface zoom policy and schedule refinement. */
            set(view: MarsSurfaceView, value: number, reset?: boolean, kind?: number,
                event?: WheelEvent | MarsSurfaceGesture | null): boolean;

            /** @brief Schedule refinement for the current surface view. */
            schedule(view: MarsSurfaceView): void;

            /** @brief Accept a saved camera after native refinement. */
            accept(saved: MarsSurfaceSnapshot | null): MarsSurfaceSnapshot | null;
        };
        readonly preview: {
            /** @brief Prepare browser rendering resources for an SVG chart. */
            prepare(chart: SVGSVGElement | null, saved?: MarsSurfaceSnapshot | null, software?: boolean): MarsSurfaceView | null;

            /** @brief Draw the chart using native-projected geometry. */
            draw(chart: SVGSVGElement, angles: MarsSurfaceAngles, bounds?: number[] | null): boolean;

            /** @brief Apply a native pinch update and redraw the chart. */
            pinch(chart: SVGSVGElement, state: MarsSurfaceGesture, angles: MarsSurfaceAngles): boolean;

            /** @brief Apply native rotation to the current gesture. */
            rotate(chart: SVGSVGElement, state: MarsSurfaceGesture, angles: MarsSurfaceAngles): boolean;

            /** @brief Release surface preview resources. */
            dispose(): void;

            /** @brief Clear the retained surface preview when permitted. */
            clear(force?: boolean): void;

            /** @brief Capture the currently retained surface camera. */
            snapshot(): MarsSurfaceSnapshot | null;
        };
        readonly document: Document;
        readonly flow: MarsApiFlowPort;
        readonly requests: MarsApiRequestPort;
    }

    /** @brief Narrow, lazily supplied dependencies of SolverRenderController. */
    export interface MarsApiSolverRenderControllerDependencies {
        readonly workspace: {
            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;
        };
        readonly results: {
            /** @brief Schedule solver layout fitting or the supplied frame callback. */
            scheduleSolverTexFit(callback?: FrameRequestCallback): void;
        };
        readonly native: MarsApiNativePort;
        readonly flow: MarsApiFlowPort;
        readonly requests: MarsApiRequestPort;
    }

    /** @brief Narrow, lazily supplied dependencies of CalendarRequestController. */
    export interface MarsApiCalendarRequestControllerDependencies {
        readonly locations: {
            /** @brief Refresh DateTime location with optional coordinate replacement. */
            refreshDatetimeJurisdictionLocation(options?: WorksheetCalendarOptions): Promise<void>;
        };
        readonly results: {
            /** @brief Attach totality actions within the supplied result node. */
            bindAlmanacTotalityActions(node?: HTMLElement): void;
        };
        readonly context: {controlToken: string};
        readonly native: MarsApiNativePort;
        readonly http: MarsApiHttpPort;
        readonly application: {configuration: LabConfiguration};
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of EvaluationController. */
    export interface MarsApiEvaluationControllerDependencies {
        readonly workspace: {
            /** @brief Resolve a value to its native worksheet mode index. */
            workspaceModeId(mode: unknown): number;

            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;

            /** @brief Retain an entry through native history policy. */
            pushExpressionHistory(entry: unknown): void;

            /** @brief Reveal the result pane. */
            showResults(): void;

            /** @brief Refresh backward and forward history availability. */
            updateHistoryButtons(): void;

            /** @brief Read the requested numerical precision. */
            requestedValuePrecision(): number;
        };
        readonly bindings: {
            /** @brief Read the current authored expression. */
            currentExpressionText(): string;

            /** @brief Capture authored values from visible binding inputs. */
            visibleBindingValues(): WorksheetBinding[];

            /** @brief Commit visible binding inputs while ownership remains current. */
            commitVisibleBindingInputs(isCurrent?: WorksheetGuard): Promise<boolean | undefined>;

            /** @brief Assemble authored expression and visible bindings through native policy. */
            expressionWithVisibleBindings(sourceExpression: string, visibleBindings: WorksheetBinding[]): Promise<string>;
        };
        readonly locations: {
            /** @brief Capture native-normalised DateTime controls. */
            currentDatetimeState(): WorksheetCalendarState;

            /** @brief Obtain native summary text for DateTime state. */
            datetimeSummaryText(state?: WorksheetCalendarState): string;

            /** @brief Obtain native summary text for almanac state. */
            almanacSummaryText(state?: WorksheetCalendarState): string;

            /** @brief Capture native-normalised almanac controls. */
            currentAlmanacState(): WorksheetCalendarState;
        };
        readonly persistence: {
            /** @brief Capture a native history snapshot with an optional source override. */
            historyStateForMode(mode?: WorksheetMode, textOverride?: string | null): WorksheetSnapshot;

            /** @brief Read the native predecessor of a snapshot. */
            previousModeStateForHistory(nextState: WorksheetSnapshot | null): WorksheetSnapshot | null;

            /** @brief Commit a captured worksheet snapshot to native history. */
            commitModeState(mode?: WorksheetMode, textOverride?: string | null): void;
        };
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of GoalSeekController. */
    export interface MarsApiGoalSeekControllerDependencies {
        readonly workspace: {
            /** @brief Resolve a value to its native worksheet mode index. */
            workspaceModeId(mode: unknown): number;

            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;
        };
        readonly bindings: {
            /** @brief Read the current authored expression. */
            currentExpressionText(): string;

            /** @brief Prepare the goal expression and starting values natively. */
            goalSeekExpressionAndStarts(sourceExpression: string, providedStart?: MarsApiStartValues): Promise<LabBindingGoalStart>;

            /** @brief Read the native-selected expression body. */
            expressionBodyForEditor(text: string): string;

            /** @brief Assemble expression and bindings without browser algebra. */
            expressionWithBindings(bodyText: string, bindings: WorksheetBinding[]): Promise<string>;

            /** @brief Return expression text or the binding adapter's empty fallback. */
            expressionTextOrEmpty(text: string): string;
        };
        readonly native: MarsApiNativePort;
        readonly flow: MarsApiFlowPort;
    }

    /** @brief Narrow, lazily supplied dependencies of NativeFlowRuntime. */
    export interface MarsApiNativeFlowRuntimeDependencies {
        readonly workspace: {
            /** @brief Read the selected worksheet mode. */
            currentMode(): WorksheetMode;
        };
        readonly results: {
            readonly labEvaluationView: object;
        };
        readonly native: MarsApiContinuationPort;
        readonly requests: MarsApiRequestPort;
    }

    // Request resources and mutable frames exchanged with native workflow stages.

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
    export interface MarsApiData {
        editor?: WorksheetEditorMetadata;
        rows?: WorksheetIntegratorRow[];
        binding_values?: WorksheetBinding[];
        evaluation_ready?: boolean;
        ok?: boolean;
        partial_error?: unknown;
        [key: string]: unknown;
    }

    /** @brief Decoded native payload paired with its original transport response. */
    export interface MarsApiPostResult {
        response: MarsTransportResponse;
        data: MarsApiData;
    }

    /**
     * @brief Transport frame created by LabRequestManager.post and advanced by lab_flow_transport.c (65).
     *
     * The browser supplies request, payload and fallback transport inputs. Native stages assert
     * ownership, select an endpoint, fetch response, assert ownership, decode data, assert ownership,
     * install metadata, then assign result. Response/data/result are absent until their producers run.
     */
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

    /**
     * @brief Request-action frame created by run/runUI for native transport workflows 66 and 67.
     *
     * Native admission supplies request before callback invocation, awaits the action when required,
     * writes its completion value to result and arranges cleanup. Workflow 67 also selects UI error
     * recovery. These are mutable exchange records, not browser-owned state-machine objects.
     */
    export interface MarsApiRunFrame {
        operation: string;
        mode: string;
        callback: MarsApiRequestAction;
        options: MarsApiOptions;
        request?: MarsApiRequest;
        result?: unknown;
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
        current?: WorksheetGuard;
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

    /** @brief Matrix inputs captured by fetchMatrixEvaluation before native payload construction. */
    export interface MarsApiMatrixFetchFrame {
        mode: 0;
        options: MarsApiMatrixOptions;
        request: MarsApiRequest | null;
    }

    /** @brief Request ownership for a fetch whose worksheet inputs are read by native-selected services. */
    export interface MarsApiWorksheetFetchFrame {
        /** @brief Fetch kind: equation 1, differential equation 2, DateTime 3, almanac 4; not a worksheet mode index. */
        mode: 1 | 2 | 3 | 4;
        request: MarsApiRequest | null;
    }

    /**
     * @brief Inputs to native fetch continuation 15, which returns MarsApiPostResult.
     *
     * EvaluationTransport creates the appropriate record. Native C selects the payload
     * producer and endpoint, then invokes transport; only matrix inputs contain options.
     */
    export type MarsApiFetchFrame = MarsApiMatrixFetchFrame | MarsApiWorksheetFetchFrame;

    /**
     * @brief Native-selected evaluation fetch with only the inputs needed by that mode.
     * Expression text is captured before awaiting transport; other modes retain their
     * existing native input-capture workflows. Matrix callers may supply explicit overrides.
     */
    export type MarsApiFetchContext =
        | {kind: 'expression'; text: string}
        | {kind: 'matrix'; options?: MarsApiMatrixOptions}
        | {kind: 'equation' | 'diffequation' | 'integrator' | 'datetime' | 'almanac'};

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

    /** @brief Available evaluation fields supplied to result installation; omitted fields stay absent. */
    export interface MarsApiInstallContext {
        enteredBindings?: WorksheetBinding[];
        state?: WorksheetCalendarState;
        text?: string;
        editorText?: string;
        entered?: unknown;
    }

    /** @brief Opaque result and captured context supplied to native result installation. */
    export interface MarsApiInstallFrame {
        mode: number;
        data: unknown;
        context: MarsApiInstallContext;
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
        start: MarsApiRecord;
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

    // Low-level transport services used by lab_flow_transport.c.

    /** @brief Atomic browser capabilities for native transport and request continuations. */
    export interface MarsApiTransportServices {

        /** @brief Reject an obsolete or timed-out request before beginning transport work. */
        assertCurrent(request: MarsApiRequest | null): void;

        /** @brief Synchronise the worksheet mode and report whether the native request token remains live. */
        current(request: MarsApiRequest | null): boolean;

        /** @brief Finish native request ownership and release browser resources that are no longer live. */
        finish(request: MarsApiRequest | null): void;

        /** @brief Ask native C to admit the captured operation and acquire its browser resources. */
        begin(frame: MarsApiRunFrame): MarsApiRequest | null;

        /** @brief Invoke the captured operation with its admitted request; reject missing ownership. */
        callback(frame: MarsApiRunFrame): unknown;

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
        clearResults(options: MarsApiResultResetOptions): void;

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

    /** @brief Browser-supplied form inputs consumed by native continuation 12; completion is opaque. */
    export interface MarsApiFormsFrame { payload: unknown; options: MarsApiFormsOptions; }

    /** @brief Authored payload passed by requestLabPresentation; completion supplies MarsApiData. */
    export interface MarsApiPresentationRequest {
        payload: unknown;
        request: MarsApiRequest | null;
    }

    /** @brief Text passed by prepareLabEditor; native cache selection supplies WorksheetEditorMetadata. */
    export interface MarsApiEditorPreparation {
        editor: true;
        text: string;
    }

    /** @brief Native continuation 13 selects fetching or cached editor preparation from these distinct inputs. */
    export type MarsApiPresentationFrame = MarsApiPresentationRequest | MarsApiEditorPreparation;

    /** @brief Captured integral source, rows and native context used for stale-work checks. */
    export interface MarsApiIntegratorSnapshot {
        text: string;
        rows: WorksheetIntegratorRow[];
        context: number;
        rowsText: string;
    }

    /** @brief Initial request ownership consumed by integrator continuation 8, which captures and prepares its rows. */
    export interface MarsApiIntegratorFrame { request: MarsApiRequest | null; }
    // Service tables follow their producing controllers; native C selects invocation order.

    /** @brief EditorPresentationController services for presentation metadata and pending editor preparation. */
    export interface MarsApiPresentationServices extends MarsApiEditorServices {
        /** @brief Install native presentation metadata into the result caches. */
        installPresentation(data: unknown): void;

        /** @brief Request native presentation metadata without interpreting the payload. */
        requestPresentation(payload: unknown, request?: MarsApiRequest | null): Promise<MarsApiData>;
    }

    /** @brief EvaluationTransport services for captured payload inputs and decoded HTTP replies. */
    export interface MarsApiEvaluationTransportServices {
        /** @brief Post an encoded native payload to the supplied endpoint with optional cancellation. */
        rawPost(endpoint: string, payload: unknown, signal?: AbortSignal): Promise<MarsTransportResponse>;

        /** @brief Decode the response body without reinterpreting its native payload. */
        responseData(response: MarsTransportResponse): Promise<unknown>;

        /** @brief Ask native C to construct a payload using current precision and captured inputs. */
        buildPayload(kind: number, inputs: unknown[], request: MarsApiRequest | null): unknown;

        /** @brief Capture the integral source, bounds and native context for freshness checks. */
        integratorSnapshot(): MarsApiIntegratorSnapshot;

        /** @brief Read the authored integral source, falling back to the visible editor. */
        integratorText(): string;

        /** @brief Serialise the current integral-bound rows through the workspace adapter. */
        integratorRowsText(): string;

        /** @brief Refresh native integral metadata before preparing bound rows. */
        refreshIntegratorForms(rows?: WorksheetIntegratorRow[], expression?: string): Promise<WorksheetIntegratorRow[] | null | undefined>;

        /** @brief Ask the workspace adapter for the active native integral-row plan. */
        activeIntegratorRows(rows?: WorksheetIntegratorRow[], expressionText?: string): WorksheetRowPlan;

        /** @brief Project the supplied integral-bound rows into their editor controls. */
        renderIntegratorRows(rows: WorksheetIntegratorRow[]): void;

        /** @brief Ask native C to build an integral payload with the current precision and interval cap. */
        integratorPayload(text: string, bounds: WorksheetIntegratorRow[], request: MarsApiRequest | null): unknown;

        /** @brief Capture mode-specific inputs and let native C construct any mathematical payload. */
        fetchPayload(frame: MarsApiFetchFrame): unknown;

        /** @brief Fetch the native calculus result using the captured request and preview quality. */
        fetchCalculus(text: string, wrt: string, action: string, request: MarsApiRequest | null,
                      preview: boolean): Promise<MarsApiPostResult>;

        /** @brief Fetch a matrix result using explicit payload overrides and request ownership. */
        fetchMatrixOptions(options: MarsApiMatrixOptions): Promise<MarsApiPostResult>;

        /** @brief Fetch one native-selected evaluation and return its decoded response. */
        fetch(context: MarsApiFetchContext, request: MarsApiRequest | null): Promise<MarsApiPostResult>;

        /** @brief Fetch weather for captured calendar state under its parent request. */
        fetchWeather(state: WorksheetCalendarState, request: MarsApiRequest | null): Promise<MarsApiPostResult>;
    }

    /** @brief FunctionCardController services for completion, cancellation and error projection. */
    export interface MarsApiFunctionServices {
        /** @brief Cancel function-card work and clear its presentation and busy indicator. */
        clearFunctionRun(): void;

        /** @brief Report response acceptance and timeout state to the native function-card presenter. */
        functionComplete(request: MarsApiRequest, outcome: number, data: unknown): unknown;

        /** @brief Report a function-card exception while preserving ordinary message-getter behaviour. */
        functionException(request: MarsApiRequest, error: unknown): unknown;
    }

    /** @brief PlotRequestController services for calculus inputs, result projection and background plotting. */
    export interface MarsApiPlotServices {
        /** @brief Capture the active worksheet mode and retained scalar matrix expression. */
        calculusInitial(): {mode: WorksheetMode; scalar: string};

        /** @brief Snapshot visible bindings, variables and differentiability before calculus work. */
        calculusCapture(): {bindings: WorksheetBinding[]; variables: string[]; differentiable: boolean};

        /** @brief Read the retained scalar expression from the latest matrix result. */
        matrixScalar(): string;

        /** @brief Ask the binding editor to assemble authored values without client-side algebra. */
        expressionWithVisibleBindings(sourceExpression: string, visibleBindings: WorksheetBinding[]): Promise<string>;

        /** @brief Set and return the native-selected result-pane title. */
        calculusTitle(text: string): string;

        /** @brief Install native plot output while retaining the appropriate gesture projection or camera. */
        displayPlot(result: MarsApiRecord): unknown;

        /** @brief Present a plot failure through the native layout adapter. */
        plotError(error: unknown): unknown;

        /** @brief Start background plotting tied to the parent token without extending its busy lifetime. */
        automaticPlot(parent: MarsApiRequest): void;

        /** @brief Set and return the native-selected numerical-value card title. */
        valueTitle(text: string): string;

        /** @brief Project the native calculus result into its result cards. */
        displayCalculusResult(result: MarsApiData): void;

        /** @brief Install the native matrix rendering without rewriting its mathematics. */
        setMatrixPrettyResult(resultText: string, prettyText: string, element?: HTMLElement, moreButton?: WorksheetCopyButton): Promise<unknown>;

        /** @brief Replace the retained calculus-variable list and return the installed list. */
        calculusVariables(variables: string[]): string[];

        /** @brief Derive the displayed variable list from the supplied native binding metadata. */
        calculusBindingVariables(bindings: WorksheetBinding[] | null): string[];

        /** @brief Store and return whether native metadata permits differentiation. */
        calculusDifferentiable(enabled: boolean): boolean;

        /** @brief Refresh calculus controls using the retained variable list. */
        calculusButtons(): void;
    }

    /** @brief SolverRenderController services for snapshot ownership and responsive solver publication. */
    export interface MarsApiSolverServices {
        /** @brief Ask native C to prepare a responsive solver snapshot if its owner is still current. */
        solverPrepare(): MarsApiSolverSnapshot | null;

        /** @brief Attach request ownership and a native freshness guard to a solver snapshot. */
        solverPending(pending: MarsApiSolverSnapshot, request: MarsApiRequest): void;

        /** @brief Ask native C to publish solver output only for the current snapshot and request. */
        solverPublish(pending: MarsApiSolverPending, response: unknown, data: unknown, failed: boolean): unknown;

        /** @brief Release the pending solver snapshot only when it is still the same owned object. */
        solverRelease(pending: MarsApiSolverPending): void;

        /** @brief Ask native admission policy to start solver rendering. */
        renderSolver(source: unknown, parent: MarsApiRequest | null): Promise<unknown> | undefined;

        /** @brief Project native solver output and schedule result fitting. */
        solverSVG(...args: unknown[]): void;
    }

    /** @brief CalendarRequestController services for location, weather and mobile-access requests. */
    export interface MarsApiCalendarServices {
        /** @brief Fetch uncached mobile-access information with the configured control token. */
        mobileFetch(): Promise<MarsTransportResponse>;

        /** @brief Ask native C to assemble the configured land-totality request. */
        landPayload(data: unknown, cells: unknown): unknown;

        /** @brief Attach almanac totality actions to the supplied result element. */
        bindAlmanacTotalityActions(node: HTMLElement): void;

        /** @brief Start weather refresh without extending the caller's completion lifetime. */
        refreshWeather(parent: number, state: WorksheetCalendarState, overviewData: unknown): void;

        /** @brief Refresh DateTime jurisdiction location before evaluation. */
        refreshLocation(): Promise<void>;
    }

    /** @brief EvaluationController services for captured inputs, history and native result installation. */
    export interface MarsApiEvaluationServices {
        /** @brief Read the active worksheet mode at invocation time. */
        currentMode(): WorksheetMode;

        /** @brief Read the current authored expression. */
        expressionText(): string;

        /** @brief Capture current-mode history with the supplied source text. */
        historyState(text: string): WorksheetSnapshot;

        /** @brief Read the native predecessor for a captured snapshot. */
        previousHistory(state: WorksheetSnapshot): WorksheetSnapshot | null;

        /** @brief Retain a snapshot in native history. */
        pushHistory(state: WorksheetSnapshot): void;

        /** @brief Reveal the worksheet result pane. */
        showResults(): void;

        /** @brief Commit the current worksheet snapshot to native history. */
        commitModeState(): void;

        /** @brief Refresh history navigation availability. */
        updateHistoryButtons(): void;

        /** @brief Continue native result installation with captured evaluation inputs. */
        installEvaluation(mode: number, data: unknown, context?: MarsApiInstallContext, outcome?: number, request?: MarsApiRequest | null): Promise<unknown>;

        /** @brief Ask native policy to admit evaluation of the supplied mode. */
        evaluate(mode: WorksheetMode, options?: WorksheetEvaluationOptions): Promise<unknown> | undefined;

        /** @brief Read the requested numerical precision. */
        precision(): number;

        /** @brief Store and return the visible bindings captured for evaluation. */
        captureBindings(context: MarsApiEvaluationContext): WorksheetBinding[];

        /** @brief Commit visible binding inputs before evaluation. */
        commitBindings(): Promise<boolean | undefined>;

        /** @brief Capture DateTime state and its native summary in the evaluation context. */
        captureDatetime(context: MarsApiEvaluationContext): void;

        /** @brief Store and return the native summary of captured almanac state. */
        captureAlmanac(context: MarsApiEvaluationContext): string;

        /** @brief Assemble authored bindings natively and retain the resulting input text. */
        assemble(context: MarsApiEvaluationContext): Promise<string>;
    }

    /** @brief GoalSeekController services for authored goal inputs and native binding assembly. */
    export interface MarsApiGoalServices {
        /** @brief Prepare goal expression and starting values using native binding policy. */
        goalPrepare(sourceExpression: string, providedStart?: MarsApiStartValues): Promise<LabBindingGoalStart>;

        /** @brief Obtain the native-selected expression body for the editor. */
        goalEditorBody(text: string): string;

        /** @brief Assemble expression and bindings without browser-side algebra. */
        expressionWithBindings(bodyText: string, bindings: WorksheetBinding[]): Promise<string>;

        /** @brief Normalise absent goal expression text through the binding adapter. */
        goalExpression(text: string): string;
    }

    /** @brief Request admission, freshness, transport and nested request-owned continuations. */
    export interface MarsApiRequestServices {
        /** @brief Admit an operation under native request policy and acquire its browser resources. */
        requestBegin(operation: string, mode?: string, options?: MarsApiOptions): MarsApiRequest | null;

        /** @brief Admit an operation using the worksheet mode at the time of the call. */
        requestBeginCurrent(operation: string, options: MarsApiOptions): MarsApiRequest | null;

        /** @brief Finish the supplied native request and reap its obsolete browser resources. */
        requestFinish(request: MarsApiRequest | null | undefined): void;

        /** @brief Cancel the named operation's native channel and release its obsolete resources. */
        requestCancel(operation: string): void;

        /** @brief Read the latest unsigned main-request token for freshness checks. */
        requestLatestMain(): number;

        /** @brief Run the selected continuation stage with native-owned admission and UI error recovery. */
        runRequestFlow(kind: number, frame: object, stage: number, operation: string, mode: WorksheetMode,
                       options: MarsApiOptions): Promise<unknown>;

        /** @brief Check whether the request still owns its native channel. */
        requestCurrent(request: MarsApiRequest | null): boolean;

        /** @brief Classify response acceptance using native request policy. */
        requestOutcome(request: MarsApiRequest | null, response: MarsApiResponseStatus | null, data: MarsApiData | null, result?: boolean): number;

        /** @brief Validate required input against current request ownership. */
        requestInput(request: MarsApiRequest, text: unknown, variable?: boolean): boolean;

        /** @brief Run native endpoint selection, posting and response decoding. */
        post(request: MarsApiRequest | null, payload: unknown, fallback: string, signal?: AbortSignal): Promise<MarsApiPostResult>;

        /** @brief Read the current unsigned worksheet context identifier. */
        requestContext(): number;

        /** @brief Synchronise native request ownership with a changed worksheet mode. */
        modeChanged(mode: WorksheetMode): void;
    }

    /** @brief Native entry-point dispatch, effect execution and opaque continuation values. */
    export interface MarsApiNativeServices {
        /** @brief Invoke an atomic native operation selected by the continuation. */
        native(entry: string, ...args: unknown[]): unknown;

        /** @brief Execute native effects through the assembled own-property registry. */
        effects(plan: NativeServicePlan): unknown;

        /** @brief Return a value unchanged so the native continuation can await it. */
        awaitValue(value: unknown): unknown;

        /** @brief Give drawing and input a browser turn before installing worksheet results. */
        yieldToDrawing(): Promise<void>;
    }

    /** @brief Native-selected cancellation and exception propagation without payload reinterpretation. */
    export interface MarsApiErrorServices {
        /** @brief Throw an AbortError carrying the native-selected diagnostic. */
        abort(message: string): never;

        /** @brief Throw an ordinary Error with the native-selected diagnostic. */
        fail(message: string): never;

        /** @brief Propagate an exception without changing its identity. */
        rethrow(error: unknown): never;
    }

}
