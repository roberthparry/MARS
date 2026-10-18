/**
 * @file api.ts
 * @brief Own browser request resources and expose native worksheet workflows.
 *
 * WorksheetRequests manages abort signals, deadlines, pending editor promises and
 * plot transport queues. Native C selects endpoints, continuation stages, freshness
 * policy and mathematical operations; this module does not interpret expressions.
 * The classic-script bootstrap supplies labWire and labDOM before this file loads.
 * Controllers retain narrow, lazily supplied dependencies so definition scripts can
 * load before workspace initialisation. NativeFlowRuntime binds capability providers
 * once and rejects duplicate service keys. Callers use the owning controller directly;
 * bound public methods retain their receiver when registered as browser callbacks.
 */
namespace WorksheetRequests {

    /** Owns the abort signal, deadline timer and button lease for one native request. */
    class LabRequestResource implements MarsApiRequest {

        /** @brief Abort controller belonging exclusively to this request. */
        public readonly controller = new AbortController();

        /** @brief Browser deadline timer, absent when the native request has no deadline. */
        public timer: number | undefined;

        /** @brief Allocate browser resources for an already admitted native request. */
        public constructor(
            public readonly channel: number,
            public readonly token: number,
            public readonly button: HTMLButtonElement | null,
            public readonly disableButton: boolean,
            private readonly setRunning: (button: HTMLButtonElement, running: boolean) => void
        ) {}

        /** @brief Acquire the button indicator and arm the native-selected deadline. */
        public activate(deadline: () => number, expire: () => boolean): void {
            if (this.button) this.setRunning(this.button, true);
            if (this.disableButton && this.button) this.button.disabled = true;
            const milliseconds = deadline();
            if (milliseconds) this.timer = setTimeout(() => {
                if (expire()) this.controller.abort();
            }, milliseconds);
        }

        /** @brief Cancel the timer and transport, then release the button indicator. */
        public release(): void {
            clearTimeout(this.timer);
            this.controller.abort();
            if (this.button) this.setRunning(this.button, false);
            if (this.disableButton && this.button) this.button.disabled = false;
        }
    }

    /** Native request ownership, transport and presentation capabilities used by one manager. */
    interface RequestManagerHost {
        /** Borrow the live native request ABI. */
        exports(): MarsTransportExports;
        /** Read the active worksheet without retaining a duplicate mode. */
        mode(): WorksheetMode;
        /** Project aggregate request availability. */
        busy(value: boolean): void;
        /** Project a request's button lease. */
        running(button: HTMLButtonElement, value: boolean): void;
        /** Invalidate the current solver presentation under native admission policy. */
        invalidateSolver(): void;
        /** Resolve the default evaluation button when a request is admitted. */
        button(): HTMLElement | null;
        /** Encode a native request payload without interpreting its mathematics. */
        encode(value: unknown): Uint8Array<ArrayBuffer>;
        /** Send a native request, preserving cancellation and response status. */
        fetch(url: string, options: RequestInit): Promise<MarsTransportResponse>;
        /** Execute transport continuations with the manager's own service table. */
        flow(kind: number, frame: object, view: unknown, registry: NativeServiceRegistry,
             signal?: AbortSignal): Promise<unknown>;
        /** Install native-produced presentation metadata. */
        presentation(value: MarsApiData): void;
        /** Install native-produced function syntax metadata. */
        syntax(value: MarsApiData): void;
        /** Display a native request failure. */
        error(message: string): void;
        /** Reset the rendered result's precision control. */
        resetDigits(): void;
        /** Clear result cards under native recovery policy. */
        clearResults(options: MarsApiResultResetOptions): void;
        /** Display the native-selected worksheet status. */
        status(message: string): void;
    }

    /** Browser resources only; native code owns tokens, routing and lifecycle policy. */
    class LabRequestManager {
        private readonly lifetime = new AbortController();
        private readonly modes: Readonly<Record<string, number>> = Object.freeze({
            expression: 0, equation: 1, diffequation: 2, matrix: 3, integrator: 4, datetime: 5, almanac: 6
        });
        private readonly resources = new Map<number, LabRequestResource>();
        private readonly decoder = new TextDecoder('utf-8', {fatal: true});
        private readonly operations: Map<string, number>;
        private readonly operationCount: number;
        private readonly mainChannel: number;

        /** @brief Load the native operation catalogue and bind detachable public capabilities. */
        public constructor(private readonly host: RequestManagerHost) {
            // Retain detachable, enumerable public capabilities used by host adapters and snapshots.
            marsBindCallbacks(this, [
                'begin', 'current', 'finish', 'cancel', 'modeChanged', 'busy', 'post', 'run', 'runUI', 'outcome',
                'context', 'latestMain', 'action', 'flags', 'timedOut', 'input', 'dispose'
            ]);
            this.operationCount = this.native().lab_request_operation_count();
            this.operations = new Map(Array.from({length: this.operationCount},
                (_, index) => [this.catalogueText(0, index), index]));
            this.mainChannel = this.native().lab_request_channel(this.operations.get('evaluate') ?? this.operationCount);
        }

        private native(): MarsTransportExports { return this.host.exports(); }

        /** @brief Finish only tokens owned by this manager and release their browser resources. */
        public dispose(): void {
            this.lifetime.abort();
            for (const request of this.resources.values()) {
                if (this.native().lab_request_live(request.channel, request.token))
                    this.native().lab_request_finish(request.channel, request.token);
                request.release();
            }
            this.resources.clear();
        }

        private catalogueText(kind: number, index: number): string {
            const controller = this.native();
            return this.decoder.decode(new Uint8Array(controller.memory.buffer,
                controller.lab_request_text(kind, index), controller.lab_request_text_length(kind, index)));
        }

        /** @brief Read the native aggregate busy state. */
        public busy(): boolean { return Boolean(this.native().lab_request_busy()); }

        private reap(): void {
            // The map is bounded by the native request-channel catalogue.
            for (const [channel, request] of this.resources) {
                if (this.native().lab_request_live(channel, request.token)) continue;
                request.release();
                this.resources.delete(channel);
            }
        }

        /** @brief Synchronise the native mode and reap resources whose tokens are no longer live. */
        public modeChanged(mode: string): void {
            this.native().lab_request_set_mode(this.modes[mode] ?? 7);
            this.reap();
            this.host.busy(this.busy());
        }

        /** @brief Check request freshness after synchronising the current worksheet mode. */
        public current(request: MarsApiRequest | null | undefined): boolean {
            this.modeChanged(this.host.mode());
            return Boolean(request && this.native().lab_request_live(request.channel, request.token));
        }

        /** @brief Ask C to admit a request, then acquire its browser resources. */
        public begin(operation: string, mode: string = this.host.mode(), options: MarsApiOptions = {}): MarsApiRequest | null {
            this.modeChanged(this.host.mode());
            const action = this.operations.get(operation) ?? this.operationCount;
            const channel = this.native().lab_request_channel(action);
            const token = this.native().lab_request_begin(this.modes[mode] ?? 7, action,
                Number(options.input !== false), Number(options.variable !== false),
                Number(Boolean(options.scalar)), options.parent || 0) >>> 0;
            if (!token) return null;
            const policy = this.native().lab_transport_policy(action, channel, this.mainChannel);
            if (policy & 2) this.host.invalidateSolver();
            this.reap();
            const button = options.button || (policy & 1 ? this.host.button() : null);
            if (button && !(marsTransportIsDOM(button, 'HTMLButtonElement'))) throw new TypeError('Expected a request button');
            const request = new LabRequestResource(channel, token, button, !!options.disableButton, (target, running) => this.host.running(target, running));
            this.resources.set(channel, request);
            request.activate(() => this.native().lab_request_deadline(channel, token),
                             () => Boolean(this.native().lab_request_timeout(channel, token)));
            this.host.busy(this.busy());
            return request;
        }

        /** @brief Finish native ownership and release resources that are no longer live. */
        public finish(request: MarsApiRequest | null | undefined): void {
            if (request) this.native().lab_request_finish(request.channel, request.token);
            this.reap();
            this.host.busy(this.busy());
        }

        /** @brief Cancel an operation's native channel and reap its browser resources. */
        public cancel(operation: string): void {
            const channel = this.native().lab_request_channel(this.operations.get(operation) ?? this.operationCount);
            this.native().lab_request_cancel(channel);
            if (this.native().lab_transport_policy(this.operationCount, channel, this.mainChannel) & 2)
                this.host.invalidateSolver();
            this.reap();
            this.host.busy(this.busy());
        }

        /** @brief Reject obsolete or timed-out request ownership before transport work. */
        public assertCurrent(request: MarsApiRequest | null): void {
            if (request && !this.current(request)) throw new DOMException('Obsolete Lab request', 'AbortError');
            if (request && this.timedOut(request)) throw new DOMException('Lab request timed out', 'AbortError');
        }

        /** @brief Ask native C to classify the response against the current request. */
        public outcome(request: MarsApiRequest | null, response: MarsApiResponseStatus | null,
                       data: MarsApiData | null, result = true): number {
            if (!request || !this.current(request)) return 0;
            return this.native().lab_request_accept(request.channel, request.token, Number(Boolean(response?.ok)),
                Number(Boolean(data?.ok)), Number(Boolean(result)), Number(Boolean(data?.partial_error)));
        }

        private readonly transportServices = Object.freeze({
            assertCurrent: (request: MarsApiRequest | null) => this.assertCurrent(request),
            current: (request: MarsApiRequest | null) => this.current(request),
            finish: (request: MarsApiRequest | null) => this.finish(request),
            begin: (frame: MarsApiRunFrame) => this.begin(frame.operation, frame.mode, frame.options),
            callback: (frame: MarsApiRunFrame) => {
                if (!frame.request) throw new Error('Missing native request');
                return frame.callback(frame.request);
            },
            endpoint: (request: MarsApiRequest) =>
                this.catalogueText(1, this.native().lab_request_endpoint(request.channel, request.token)),
            fetch: (endpoint: string, frame: MarsApiPostFrame) => this.host.fetch(endpoint, {
                method: 'POST', body: this.host.encode(frame.payload),
                signal: frame.request ? frame.request.controller.signal : frame.signal
            }),
            decode: (response: MarsTransportResponse) => response.labData(),
            presentation: (frame: {data: MarsApiData}) => this.host.presentation(frame.data),
            syntax: (frame: {data: MarsApiData}) => this.host.syntax(frame.data),
            result: (frame: MarsApiDecodedPostFrame) => ({response: frame.response, data: frame.data}),
            invalidEndpoint: () => { throw new Error('Invalid native request endpoint'); },
            rethrow: (frame: {exception: unknown}) => { throw frame.exception; },
            renderError: (error: string) => this.host.error(error),
            resetDigits: () => this.host.resetDigits(),
            clearResults: (options: MarsApiResultResetOptions) => this.host.clearResults(options),
            status: (text: string) => this.host.status(text)
        } satisfies MarsApiTransportServices);

        private async advance(kind: number, frame: {result?: unknown}): Promise<unknown> {
            await this.host.flow(kind, frame, null, this.transportServices, this.lifetime.signal);
            return frame.result;
        }

        /** @brief Run native endpoint selection, transport, decoding and metadata installation. */
        public async post(request: MarsApiRequest | null, payload: unknown, fallback: string,
                          signal?: AbortSignal): Promise<MarsApiPostResult> {
            const frame: MarsApiPostFrame = {request, payload, fallback, signal};
            const result = await this.advance(65, frame) as MarsApiPostResult | undefined;
            if (!result) throw new Error('Native request did not return a response');
            return result;
        }

        /** @brief Run an admitted action with native cleanup, preserving its resolved result type. */
        public run<T>(operation: string, mode: string, callback: MarsApiRequestAction<T>,
                      options: MarsApiOptions = {}): Promise<T | undefined> {
            const frame: MarsApiRunFrame = {operation, mode, callback, options};
            return this.advance(66, frame) as Promise<T | undefined>;
        }

        /** @brief Run an owned callback with native-selected UI error recovery. */
        public runUI<T>(operation: string, mode: string, callback: MarsApiRequestAction<T>,
                        options: MarsApiOptions = {}): Promise<T | undefined> {
            const frame: MarsApiRunFrame = {operation, mode, callback, options};
            return this.advance(67, frame) as Promise<T | undefined>;
        }

        /** @brief Read the current unsigned native worksheet context identifier. */
        public context(): number { return this.native().lab_request_context() >>> 0; }

        /** @brief Read the latest unsigned main-request token. */
        public latestMain(): number { return this.native().lab_request_latest_main() >>> 0; }

        /** @brief Read the native action name associated with a request. */
        public action(request: MarsApiRequest): string {
            return this.catalogueText(2, this.native().lab_request_action(request.channel, request.token));
        }

        /** @brief Read the native policy flags associated with a request. */
        public flags(request: MarsApiRequest): number { return this.native().lab_request_flags(request.channel, request.token); }

        /** @brief Check whether native C recorded expiry of the request deadline. */
        public timedOut(request: MarsApiRequest): boolean {
            return Boolean(this.native().lab_request_timed_out(request.channel, request.token));
        }

        /** @brief Validate input availability against current native request ownership. */
        public input(request: MarsApiRequest, text: unknown, variable = true): boolean {
            return this.current(request) && Boolean(this.native().lab_request_input(
                request.channel, request.token, Number(Boolean(text)), Number(Boolean(variable))));
        }
    }

    /** @brief Own editor preparation promises and native presentation requests. */
    export class EditorPresentationController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiEditorPresentationControllerDependencies) {
            marsBindCallbacks(this, [
                'requestLabForms', 'requestLabPresentation', 'prepareLabEditor', 'dispose', 'capabilities'
            ]);
        }

        /** Native text preparation; callers retain transport errors and can await edit boundaries. */
        public requestLabForms<T = unknown>(payload: unknown, options: MarsApiFormsOptions = {}): Promise<T> {
            const frame: MarsApiFormsFrame = {payload, options};
            return this.dependencies.flow.continue(12, frame) as Promise<T>;
        }

        /** @brief Request presentation metadata without interpreting the supplied native payload. */
        public requestLabPresentation(
            payload: unknown, request: MarsApiRequest | null = null): Promise<MarsApiData> {
            const frame: MarsApiPresentationFrame = {payload, request};
            return this.dependencies.flow.continue(13, frame) as Promise<MarsApiData>;
        }

        /** @brief Prepare editor metadata through the native cache and context-freshness workflow. */
        public prepareLabEditor(text: string): Promise<WorksheetEditorMetadata> {
            const frame: MarsApiPresentationFrame = {editor: true, text};
            return this.dependencies.flow.continue(13, frame) as Promise<WorksheetEditorMetadata>;
        }

        /** @brief Pending editor promises; C decides cache eligibility and freshness. */
        public readonly pending = new Map<string, MarsApiEditorPending>();

        /** @brief Release this instance's cache references without cancelling shared transport. */
        public dispose(): void {
            this.pending.clear();
        }

        private start(source: string, pending: MarsApiEditorPending,
                      payload: MarsApiEditorPayload): Promise<WorksheetEditorMetadata> {
            const promise = this.requestLabPresentation(payload).then(data => {
                if (!data.editor) throw new Error('Missing native editor presentation');
                return data.editor;
            }).finally(() => {
                if (this.pending.get(source) === pending) this.pending.delete(source);
            });
            pending.promise = promise;
            this.pending.set(source, pending);
            return promise;
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                installPresentation: (data: unknown) => this.dependencies.results.installLabPresentationData(data),
                requestPresentation: (payload: unknown, request?: MarsApiRequest | null) =>
                    this.requestLabPresentation(payload, request),
                editorRequests: () => this.pending,
                editorPromise: (source: string, pending: MarsApiEditorPending, payload: MarsApiEditorPayload) => this.start(source, pending, payload)
            } satisfies MarsApiPresentationServices;
        }

    }

    /** @brief Capture worksheet payload inputs and transport native evaluation replies. */
    export class EvaluationTransport {
        /** Native fetch-continuation selectors for modes without extra captured inputs. */
        private static readonly worksheetFetchModes = {equation: 1, diffequation: 2, datetime: 3, almanac: 4} as const;

        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiEvaluationTransportDependencies) {
            marsBindCallbacks(this, [
                'fetch', 'fetchEvaluation', 'fetchMatrixEvaluation', 'fetchEquationEvaluation',
                'fetchDiffequationEvaluation', 'fetchIntegratorEvaluation', 'fetchDatetimeEvaluation',
                'fetchDatetimeWeather', 'fetchAlmanacEvaluation', 'capabilities'
            ]);
        }

        /** @brief Build and post an expression or calculus payload using native request formatting. */
        public async fetchEvaluation(text: string, wrt = '', action = '', bindingSource: unknown = '', bindingValue = '', request: MarsApiRequest | null = null,
                                       preview = false): Promise<MarsApiPostResult> {
            return this.dependencies.requests.post(
                request,
                this.dependencies.native.call(
                    'lab_payload_build', 0, [text, bindingSource, bindingValue, wrt, action, this.dependencies.workspace.currentMode(), preview,
                                            this.dependencies.workspace.currentMode() === 'integrator' ? this.dependencies.workspace.currentIntegratorBoundRows() : []], request,
                    this.dependencies.workspace.requestedValuePrecision(), this.dependencies.context.worksheetSession.editor.lastExpressionUpdatedAt),
                '/eval');
        }

        /** @brief Execute the native-selected fetch while preserving its mode-specific capture boundary. */
        public fetch(context: MarsApiFetchContext, request: MarsApiRequest | null): Promise<MarsApiPostResult> {
            if (context.kind === 'expression')
                return this.fetchEvaluation(context.text, '', '', '', '', request);
            if (context.kind === 'integrator')
                return this.dependencies.flow.continue(8, {request}) as Promise<MarsApiPostResult>;

            let frame: MarsApiFetchFrame;
            if (context.kind === 'matrix') {
                frame = {mode: 0, options: context.options ?? {}, request};
            } else {
                frame = {mode: EvaluationTransport.worksheetFetchModes[context.kind], request};
            }
            return this.dependencies.flow.continue(15, frame) as Promise<MarsApiPostResult>;
        }

        /** @brief Fetch a matrix result using explicit matrix options and native persistence policy. */
        public fetchMatrixEvaluation(options: MarsApiMatrixOptions = {}): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'matrix', options}, options.request || null);
        }

        /** @brief Fetch the current equation result through its native continuation. */
        public fetchEquationEvaluation(request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'equation'}, request);
        }

        /** @brief Fetch the current differential-equation result through its native continuation. */
        public fetchDiffequationEvaluation(request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'diffequation'}, request);
        }

        /** @brief Prepare integral bounds and fetch the native numerical integration result. */
        public fetchIntegratorEvaluation(request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'integrator'}, request);
        }

        /** @brief Fetch the native datetime result within the supplied request lifetime. */
        public fetchDatetimeEvaluation(request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'datetime'}, request);
        }

        /** @brief Fetch weather for the captured calendar state with optional cancellation. */
        public async fetchDatetimeWeather(state: WorksheetCalendarState, signal?: AbortSignal,
                                            request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.dependencies.requests.post(
                request, this.dependencies.native.call('lab_payload_build', 6, [state], request, 0, 0), '/datetime-weather', signal);
        }

        /** @brief Fetch the native almanac result within the supplied request lifetime. */
        public fetchAlmanacEvaluation(request: MarsApiRequest | null = null): Promise<MarsApiPostResult> {
            return this.fetch({kind: 'almanac'}, request);
        }

        /** @brief Capture browser inputs for the native request workflow. */
        private labApiFetchPayload(frame: MarsApiFetchFrame): unknown {
            if (frame.mode === 0)
                return this.dependencies.native.call('lab_payload_build', 1,
                    [frame.options, this.dependencies.bindings.currentExpressionText(), frame.options.operand !== undefined],
                    frame.request, this.dependencies.workspace.requestedValuePrecision(), 0);
            return this.payloads[frame.mode](frame);
        }

        private readonly payloads = Object.freeze({
            1: (frame: MarsApiFetchFrame) =>
                this.dependencies.native.call('lab_payload_build', 2, [this.dependencies.bindings.currentExpressionText()], frame.request, this.dependencies.workspace.requestedValuePrecision(), 0),
            2: (frame: MarsApiFetchFrame) =>
                this.dependencies.native.call('lab_payload_build', 3, [this.dependencies.bindings.currentExpressionText()], frame.request, this.dependencies.workspace.requestedValuePrecision(), 0),
            3: (_frame: MarsApiFetchFrame) => this.dependencies.locations.currentDatetimeState(),
            4: (_frame: MarsApiFetchFrame) => this.dependencies.locations.currentAlmanacState()
        });

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                rawPost: (endpoint: string, payload: unknown, signal?: AbortSignal) =>
                    this.dependencies.http.fetch(endpoint, {method: 'POST', body: this.dependencies.wire.encode(payload), signal}),
                responseData: (response: MarsTransportResponse) => response.labData(),
                buildPayload: (kind: number, inputs: unknown[], request: MarsApiRequest | null) =>
                    this.dependencies.native.call('lab_payload_build', kind, inputs, request, this.dependencies.workspace.requestedValuePrecision(), 0),
                integratorSnapshot: () => {
                    const text = this.dependencies.bindings.currentExpressionText() || this.dependencies.context.expr.value.trim();
                    const rows = this.dependencies.workspace.currentIntegratorRows();
                    const context = this.dependencies.requests.context();
                    const rowsText = this.dependencies.workspace.integratorBoundsTextFromRows(rows);
                    return {text, rows, context, rowsText};
                },
                integratorText: () => this.dependencies.bindings.currentExpressionText() || this.dependencies.context.expr.value.trim(),
                integratorRowsText: () => this.dependencies.workspace.integratorBoundsTextFromRows(this.dependencies.workspace.currentIntegratorRows()),
                refreshIntegratorForms: (rows?: WorksheetIntegratorRow[], expression?: string) =>
                    this.dependencies.workspace.refreshIntegratorForms(rows, expression),
                activeIntegratorRows: (rows?: WorksheetIntegratorRow[], expression?: string) =>
                    this.dependencies.workspace.activeIntegratorRowPlan(rows, expression),
                renderIntegratorRows: (rows: WorksheetIntegratorRow[]) => this.dependencies.bindings.renderIntegratorRows(rows),
                integratorPayload: (text: string, bounds: WorksheetIntegratorRow[], request: MarsApiRequest | null) => this.dependencies.native.call(
                    'lab_payload_build', 4, [text, bounds, this.dependencies.workspace.requestedIntegratorIntervalCap()], request,
                    this.dependencies.workspace.requestedValuePrecision(), 0),
                fetchPayload: (frame: MarsApiFetchFrame) => this.labApiFetchPayload(frame),
                fetchCalculus: (text: string, wrt: string, action: string, request: MarsApiRequest | null, preview: boolean) => this.fetchEvaluation(text, wrt, action, '', '', request, preview),
                fetchMatrixOptions: (options: MarsApiMatrixOptions) => this.fetchMatrixEvaluation(options),
                fetch: this.fetchCurrent.bind(this),
                fetchWeather: (state: WorksheetCalendarState, request: MarsApiRequest | null) => this.fetchDatetimeWeather(state, undefined, request)
            } satisfies MarsApiEvaluationTransportServices;
        }

        /** @brief Resolve the current fetch adapter when the retained native service is invoked. */
        private fetchCurrent(context: MarsApiFetchContext, request: MarsApiRequest | null): Promise<MarsApiPostResult> {
            return this.fetch(context, request);
        }

    }

    /** @brief Coordinate the function card request lifetime and completion capabilities. */
    export class FunctionCardController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiFunctionCardControllerDependencies) {
            marsBindCallbacks(this, ['clearFunctionRun', 'runFunctionCard', 'capabilities']);
        }

        /** @brief Cancel function-card work and clear its native presentation and busy indicator. */
        public clearFunctionRun(): void {
            this.dependencies.requests.cancel('function');
            this.dependencies.native.call('lab_function_clear');
            this.dependencies.workspace.setActionRunning(this.dependencies.context.functionRun, false);
        }

        /** @brief Execute the native function-card workflow. */
        public runFunctionCard(): Promise<unknown> {
            return this.dependencies.flow.continue(6, {});
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                clearFunctionRun: () => this.clearFunctionRun(),
                functionComplete: (request: MarsApiRequest, outcome: number, data: unknown) =>
                    this.dependencies.native.call('lab_function_complete', outcome, Number(this.dependencies.requests.timedOut(request)), data, null),
                functionException: (request: MarsApiRequest, error: unknown) =>
                    this.dependencies.native.call('lab_function_complete', 4, Number(this.dependencies.requests.timedOut(request)), null, {
                        get message() {
                            // Preserve ordinary property access, including inherited/throwing getters and
                            // null or undefined message values. Do not normalise the exception itself.
                            if (error === null || error === undefined)
                                throw new TypeError('Cannot read message from a null or undefined exception');
                            const message: unknown = Reflect.get(Object(error), 'message');
                            return String(message);
                        }
                    })
            } satisfies MarsApiFunctionServices;
        }

    }

    /** @brief Own plot request queues, gesture resources and retained-preview coordination. */
    export class PlotRequestController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiPlotRequestControllerDependencies) {
            marsBindCallbacks(this, [
                'cancelScheduledControls', 'dispose', 'schedulePlotSlice', 'takeCalculus', 'takeDerivative',
                'takeIntegral', 'takePlot', 'schedulePlotControls', 'schedulePlotWheelEnd', 'interruptPlotDrag',
                'pauseSurfaceSampling', 'setSurfaceZoom', 'zoomSurface', 'redrawPlotDrag',
                'scheduleAutomaticPlot', 'refineSurfacePlot', 'capabilities'
            ]);
        }

        /** @brief Debounce handle shared with native surface-volume cancellation. */
        private controlsTimer: number | undefined;
        /** @brief Cancel only the owned control debounce; retain displayed plot geometry. */
        public cancelScheduledControls(): void {
            clearTimeout(this.controlsTimer);
            this.controlsTimer = undefined;
        }

        private readonly wheelTimers = new Map<MarsApiGesture, number>();
        private readonly gestureTargets = new Map<number, string>();
        private readonly frames = new Map<number, () => void>();
        private sliceTimer: number | undefined;
        private slicePending: unknown;
        private sliceRunning = false;
        private disposed = false;

        /** @brief Stop this instance's queued work without cancelling another controller's requests. */
        public dispose(): void {
            this.disposed = true;
            this.cancelScheduledControls();
            clearTimeout(this.sliceTimer);
            this.sliceTimer = undefined;
            this.slicePending = null;
            for (const timer of this.wheelTimers.values()) clearTimeout(timer);
            this.wheelTimers.clear();
            for (const [frame, complete] of this.frames) {
                cancelAnimationFrame(frame);
                complete();
            }
            this.frames.clear();
            this.gestureTargets.clear();
        }

        private nextFrame(): Promise<void> {
            if (this.disposed) return Promise.resolve();
            return new Promise(resolve => {
                const frame = requestAnimationFrame(() => {
                    this.frames.delete(frame);
                    resolve();
                });
                this.frames.set(frame, resolve);
            });
        }

        /** @brief Retain only the latest slice state, without overlapping requests. */
        public schedulePlotSlice(state: unknown, delay: number): void {
            if (this.disposed) return;
            this.slicePending = state;
            clearTimeout(this.sliceTimer);
            if (!this.sliceRunning) this.sliceTimer = setTimeout(() => this.runPlotSlice(), delay);
        }

        private async runPlotSlice(): Promise<void> {
            if (this.disposed) return;
            const state = this.slicePending;
            this.slicePending = null;
            this.sliceRunning = true;
            try {
                await this.dependencies.native.services(this.dependencies.native.call('lab_plot_controls_dispatch', 2, null, state) as NativeServicePlan, {
                    takePlot: (wrt: string, preview: boolean) =>
                        this.takeCalculus(wrt, 'plot', null, Boolean(preview), true)
                } satisfies MarsApiSliceServices);
            } finally {
                this.sliceRunning = false;
                if (!this.disposed && this.slicePending) this.sliceTimer = setTimeout(() => this.runPlotSlice(), 0);
            }
        }

        /** @brief Start a native calculus action after cancelling obsolete plot-control work. */
        public takeCalculus(wrt: string, action: string, actionButton: HTMLButtonElement | null = null,
                              preview = false, live = false, plotKind?: 'complex_curve'): Promise<unknown> {
            if (!this.dependencies.volume.requesting) this.dependencies.volume.cancel(true);
            this.cancelScheduledControls();
            const frame: MarsApiCalculusFrame = {wrt, action, actionButton, preview, live, plotKind};
            return this.dependencies.flow.continue(7, frame);
        }

        /** @brief Request the native derivative with respect to the supplied variable. */
        public async takeDerivative(wrt: string, actionButton: HTMLButtonElement | null = null): Promise<unknown> {
            return this.takeCalculus(wrt, 'derivative', actionButton);
        }

        /** @brief Request the native integral with respect to the supplied variable. */
        public async takeIntegral(wrt: string, actionButton: HTMLButtonElement | null = null): Promise<unknown> {
            return this.takeCalculus(wrt, 'integral', actionButton);
        }

        /** @brief Request the native plot for the supplied variable. */
        public async takePlot(wrt: string, actionButton: HTMLButtonElement | null = null): Promise<unknown> {
            return this.takeCalculus(wrt, 'plot', actionButton);
        }

        /** @brief Debounce control changes, leaving captured-state validation and redraw selection to C. */
        public schedulePlotControls(state: unknown, delay: number): void {
            if (this.disposed) return;
            this.dependencies.volume.cancel(true);
            clearTimeout(this.controlsTimer);
            this.controlsTimer = setTimeout(() => {
                this.controlsTimer = undefined;
                this.dependencies.native.services(this.dependencies.native.call('lab_plot_controls_dispatch', 2, null, state) as NativeServicePlan,
                                {takePlot: (wrt: string) => this.takeCalculus(wrt, 'plot', null, false, true)});
            }, delay);
        }

        /** Supply the browser timer; native code owns the delay, stale-event checks and final redraw. */
        public schedulePlotWheelEnd(state: MarsApiGesture, revision: number, delay: number): void {
            if (this.disposed) return;
            clearTimeout(this.wheelTimers.get(state));
            this.wheelTimers.set(state, setTimeout(() => {
                this.wheelTimers.delete(state);
                const plan = this.dependencies.native.call('lab_plot_drag_dispatch', 6, revision, state) as NativeServicePlan;
                this.dependencies.native.services(plan, {redrawPlotDrag: (...args: MarsApiRedrawArguments) => this.redrawPlotDrag(...args)});
            }, delay));
        }

        /** Cancel only the release-time request owned by the resumed gesture. */
        public interruptPlotDrag(state: MarsApiGesture): void {
            if (state.pendingToken && state.pendingToken === this.dependencies.requests.latestMain())
                this.dependencies.requests.cancel('plot');
            delete state.pendingToken;
        }

        /** @brief Cancel pending surface resampling while retaining the displayed geometry. */
        public pauseSurfaceSampling(): void {
            this.dependencies.volume.cancel();
        }

        /** @brief Apply a native zoom-slider action and redraw the retained surface when accepted. */
        public setSurfaceZoom(value: number | string, action = 1): void {
            this.dependencies.volume.slider(action);
            if (action === 4) return;
            const chart = this.dependencies.document.querySelector<SVGSVGElement>('#plotRendered [data-surface]');
            const view = this.dependencies.preview.prepare(chart);
            if (chart && view && this.dependencies.volume.set(view, Number(value), false, 1))
                this.dependencies.preview.draw(chart, view.angles);
        }

        /** @brief Apply wheel or reset zoom to one surface using native camera and volume policy. */
        public zoomSurface(chart: SVGSVGElement, event: WheelEvent, reset: boolean, direction: number, wheel: boolean): void {
            const view = this.dependencies.preview.prepare(chart);
            if (view && this.dependencies.volume.set(view, direction > 0 ? 1.25 : 0.8, Boolean(reset), wheel ? 2 : 0, event))
                this.dependencies.preview.draw(chart, view.angles);
        }

        /** @brief Resample native curve bounds while replacing only the originating complex-grid card. */
        public async refineSurfacePlot(variable: string, chart?: SVGSVGElement): Promise<unknown> {
            if (this.disposed || (chart && !chart.isConnected)) return;
            const targeted = chart?.dataset.segments === 'true' &&
                chart.closest<HTMLElement>('[data-complex-panel]')?.dataset.complexPanel === 'svg';
            const pending = this.takeCalculus(variable, 'plot', null, false, true,
                chart?.dataset.segments === 'true' ? 'complex_curve' : undefined);
            const token = this.dependencies.requests.latestMain();
            if (targeted) this.gestureTargets.set(token, 'svg');
            try { return await pending; }
            finally { if (targeted) this.gestureTargets.delete(token); }
        }

        /** @brief Await native plotting; C owns gesture coalescing and continuation decisions. */
        public async redrawPlotDrag(state: MarsApiGesture, variable: string, preview: boolean, refine = false): Promise<void> {
            let next: MarsApiRedrawArguments | null = [state, variable, preview, refine];
            while (next && !this.disposed) {
                const [record, wrt, coarse, refinePreview]: MarsApiRedrawArguments = next;
                const revision = record.revision || 0;
                next = null;
                let accepted = false;
                try {
                    const generation = this.dependencies.requests.latestMain();
                    const chart = this.dependencies.document.querySelector<SVGSVGElement>('#plotRendered [data-surface]');
                    if (this.previewPlotDrag(record, chart, coarse, refinePreview)) {
                        // Keep the single frame yield here so native coalescing gains no extra promise boundary.
                        await this.nextFrame();
                        accepted = !!chart?.isConnected && generation === this.dependencies.requests.latestMain();
                    } else {
                        const request = this.requestPlotDrag(record, wrt, coarse);
                        try { await request.pending; }
                        finally { this.gestureTargets.delete(request.token); }
                        if (this.disposed || revision !== (record.revision || 0)) return;
                        delete record.pendingToken;
                        accepted = request.token !== 0 && this.dependencies.requests.latestMain() === request.token;
                        if (!record.pan) this.dependencies.preview.clear();
                    }
                } finally {
                    next = this.completePlotDrag(record, revision, coarse, accepted);
                }
            }
        }

        /** @brief Attempt local rotation or pinch, retaining deep zoom previews until native refinement is due. */
        private previewPlotDrag(record: MarsApiGesture, chart: SVGSVGElement | null,
                                  coarse: boolean, refinePreview: boolean | undefined): boolean {
            // Read all range controls before preview error recovery, preserving validation failures.
            let bounds: number[] | null = null;
            if (record.rangeWheel) {
                bounds = [];
                for (const id of ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper', 'plotYLower', 'plotYUpper'])
                    bounds.push(Number(this.marsApiInputValue(id)));
            }
            let local = false;
            if (chart && !record.pan) {
                try {
                    const angles: MarsSurfaceAngles = [
                        Number(this.marsApiInputValue('plotAzimuth')),
                        Number(this.marsApiInputValue('plotElevation')),
                        Number(this.marsApiInputValue('plotRoll'))
                    ];
                    local = record.pinch ? this.dependencies.preview.pinch(chart, record, angles) :
                        this.dependencies.preview.rotate(chart, record, angles);
                } catch (_) {
                    this.dependencies.preview.dispose();
                }
            }
            return (local && !refinePreview) || !!(coarse && bounds && chart && !record.pan);
        }

        /** @brief Start remote plotting and retain the token used for gesture-specific result projection. */
        private requestPlotDrag(record: MarsApiGesture, variable: string, coarse: boolean):
            {pending: Promise<unknown>; token: number} {
            if (coarse && !record.pan) this.dependencies.preview.clear();
            const pending = this.takeCalculus(variable, 'plot', null, coarse);
            const token = this.dependencies.requests.latestMain();
            record.pendingToken = token;
            if (record.pan && record.panProjection) this.gestureTargets.set(token, record.panProjection);
            return {pending, token};
        }

        /** @brief Complete only the current gesture revision, capturing the last native-requested redraw. */
        private completePlotDrag(record: MarsApiGesture, revision: number, coarse: boolean,
                                   accepted: boolean): MarsApiRedrawArguments | null {
            if (this.disposed || revision !== (record.revision || 0)) return null;
            if ((!coarse || !accepted) && !record.pan) this.dependencies.preview.clear();
            const plan = this.dependencies.native.call('lab_plot_drag_complete', record, Number(accepted)) as NativeServicePlan;
            let next: MarsApiRedrawArguments | null = null;
            this.dependencies.native.services(plan, {
                /** @brief Retain the last coalesced redraw without recursively entering plotting. */
                redrawPlotDrag: (...args: MarsApiRedrawArguments) => next = args
            });
            if (!record.active && !record.wheel && !record.pan) {
                const chart = this.dependencies.document.querySelector<SVGSVGElement>('#plotRendered [data-surface]');
                const view = this.dependencies.preview.prepare(chart);
                if (view) this.dependencies.volume.schedule(view);
            }
            return next;
        }

        /** Start independent background work without extending the evaluation's busy lifetime. */
        public scheduleAutomaticPlot(parent: MarsApiRequest): void {
            void this.dependencies.requests.run('plot', this.dependencies.workspace.currentMode(), request => {
                const frame: MarsApiCalculusFrame = {action: 'plot', automatic: true, request};
                return this.dependencies.flow.continue(7, frame);
            }, {parent: parent.token})
                .catch(error => console.error('Background plot failed', error));
        }

        /** Read a numeric plot control without asserting an arbitrary element is an input. */
        private marsApiInputValue(id: string): string {
            const node = this.dependencies.document.getElementById(id);
            if (!(marsTransportIsDOM(node, 'HTMLInputElement'))) throw new TypeError('Missing plot input: ' + id);
            return node.value;
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                calculusInitial: () => ({mode: this.dependencies.workspace.currentMode(), scalar: this.dependencies.presentationState.lastMatrixScalarExpression}),
                calculusCapture: () => ({
                    bindings: this.dependencies.bindings.visibleBindingValues(),
                    variables: [...this.dependencies.bindingState.currentVariables],
                    differentiable: this.dependencies.bindingState.currentDifferentiable
                }),
                matrixScalar: () => this.dependencies.presentationState.lastMatrixScalarExpression,
                expressionWithVisibleBindings: (source: string, bindings: WorksheetBinding[]) =>
                    this.dependencies.bindings.expressionWithVisibleBindings(source, bindings),
                calculusTitle: (text: string) => this.dependencies.context.rightPaneTitle.textContent = text,
                displayPlot: (result: Record<string, unknown>) => {
                    const projection = this.gestureTargets.get(this.dependencies.requests.latestMain());
                    if (projection && projection !== 'svg')
                        return this.dependencies.native.call('lab_layout_plot', {...result, gestureProjection: projection}, null);
                    const camera = this.dependencies.volume.accept(this.dependencies.preview.snapshot());
                    this.dependencies.preview.dispose();
                    const value = this.dependencies.native.call('lab_layout_plot',
                        projection === 'svg' ? {...result, gestureProjection: projection} : result, null);
                    const chart = this.dependencies.document.querySelector<SVGSVGElement>('#plotRendered [data-surface]');
                    const prepare = () => {
                        if (!chart?.isConnected) return;
                        try {
                            const view = this.dependencies.preview.prepare(chart, camera);
                            if (view) this.dependencies.preview.draw(chart, view.angles);
                        }
                        catch (_) { this.dependencies.preview.dispose(); }
                    };
                    prepare();
                    return value;
                },
                plotError: (error: unknown) => this.dependencies.native.call('lab_layout_plot', null, error),
                automaticPlot: (parent: MarsApiRequest) => this.scheduleAutomaticPlot(parent),
                valueTitle: (text: string) => this.dependencies.context.valueTitle.textContent = text,
                displayCalculusResult: (result: MarsApiData) =>
                    this.dependencies.results.displayCalculusResult(result),
                setMatrixPrettyResult: (result: string, pretty: string, element?: HTMLElement, moreButton?: WorksheetCopyButton) =>
                    this.dependencies.results.setMatrixPrettyResult(result, pretty, element, moreButton),
                calculusVariables: (variables: string[]) => this.dependencies.bindingState.currentVariables = variables,
                calculusBindingVariables: (bindings: WorksheetBinding[] | null) => this.dependencies.bindingState.currentVariables = this.dependencies.workspace.variableNamesFromBindings(bindings || []),
                calculusDifferentiable: (enabled: boolean) => this.dependencies.bindingState.currentDifferentiable = enabled,
                calculusButtons: () => this.dependencies.workspace.renderDerivativeButtons(this.dependencies.bindingState.currentVariables)
            } satisfies MarsApiPlotServices;
        }

    }

    /** @brief Own responsive solver snapshots and identity-checked publication. */
    export class SolverRenderController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiSolverRenderControllerDependencies) {
            marsBindCallbacks(this, ['fitSolverTexToCard', 'dispose', 'capabilities']);
        }

        /** @brief Live native render snapshot; publishing and release check identity. */
        public pending: MarsApiSolverPending | null = null;

        /** @brief Drop this controller's pending snapshot without cancelling a shared request channel. */
        public dispose(): void {
            this.pending = null;
        }

        /** @brief Request native solver layout for the current card dimensions. */
        public fitSolverTexToCard(): Promise<unknown> {
            return this.dependencies.flow.continue(14, {});
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                solverPrepare: () => this.dependencies.native.call(
                    'lab_solver_view_prepare', this.dependencies.workspace.currentMode(), this.pending,
                    Number(this.pending && this.dependencies.requests.current(this.pending.request)), this.dependencies.native.call('lab_result_solver_owner')) as MarsApiSolverSnapshot | null,
                solverPending: (pending: MarsApiSolverSnapshot, request: MarsApiRequest) => {
                    const owned: MarsApiSolverPending = Object.assign(pending, {request});
                    this.pending = owned;
                    owned.current = () => !!this.dependencies.native.call(
                        'lab_solver_view_current', this.dependencies.workspace.currentMode(), pending, this.pending,
                        Number(this.dependencies.requests.current(request)), this.dependencies.native.call('lab_result_solver_owner'));
                },
                solverPublish: (pending: MarsApiSolverPending, response: unknown, data: unknown, failed: boolean) => this.dependencies.native.call(
                    'lab_solver_view_publish', this.dependencies.workspace.currentMode(), pending, this.pending,
                    Number(this.dependencies.requests.current(pending.request)), this.dependencies.native.call('lab_result_solver_owner'), response, data, Number(failed)),
                solverRelease: (pending: MarsApiSolverPending) => {
                    if (this.pending === pending)
                        this.pending = null;
                },
                renderSolver: (source: unknown, parent: MarsApiRequest | null) => this.dependencies.flow.run(3, {source, parent}),
                solverSVG: (...args: unknown[]) => {
                    this.dependencies.native.call('lab_evaluation_solver', ...args);
                    this.dependencies.results.scheduleSolverTexFit();
                }
            } satisfies MarsApiSolverServices;
        }

    }

    /** @brief Coordinate calendar-related background requests and mobile access. */
    export class CalendarRequestController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiCalendarRequestControllerDependencies) {
            marsBindCallbacks(this, [
                'refreshMobileAccess', 'refreshAlmanacLandTotality', 'refreshDatetimeWeather', 'capabilities'
            ]);
        }

        /** @brief Refresh native mobile-access information when its controls are available. */
        public refreshMobileAccess(): Promise<unknown> {
            return this.dependencies.flow.continue(11, {});
        }

        /** @brief Run the native land-totality request for the supplied presentation data. */
        public refreshAlmanacLandTotality(data: unknown): Promise<unknown> {
            return this.dependencies.flow.continue(9, {data});
        }

        /** @brief Start weather refresh work tied to the captured parent evaluation. */
        public refreshDatetimeWeather(parent: number, state: WorksheetCalendarState, overviewData: unknown): Promise<unknown> | undefined {
            return this.dependencies.flow.run(2, {parent, state, overviewData});
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                mobileFetch: () => this.dependencies.http.fetch(
                    '/mobile-access', {cache: 'no-store', headers: this.dependencies.context.controlToken ? {'X-Dval-Lab-Control': this.dependencies.context.controlToken} : {}}),
                landPayload: (data: unknown, cells: unknown) => this.dependencies.native.call('lab_payload_land', data, cells, this.dependencies.application.configuration),
                bindAlmanacTotalityActions: (node: HTMLElement) => this.dependencies.results.bindAlmanacTotalityActions(node),
                refreshWeather: (parent: number, state: WorksheetCalendarState, overview: unknown) => {
                    void this.refreshDatetimeWeather(parent, state, overview);
                },
                refreshLocation: () => this.dependencies.locations.refreshDatetimeJurisdictionLocation()
            } satisfies MarsApiCalendarServices;
        }

    }

    /** @brief Capture evaluation inputs and delegate evaluation and installation policy to C. */
    export class EvaluationController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiEvaluationControllerDependencies) {
            marsBindCallbacks(this, [
                'evaluateLabMode', 'installEvaluationResult', 'evaluateActiveModeOnLoad', 'evaluateExpression',
                'evaluateEquation', 'evaluateDiffequation', 'evaluateMatrix', 'evaluateIntegrator',
                'evaluateDatetime', 'evaluateAlmanac', 'capabilities'
            ]);
        }

        /** @brief Begin native evaluation for a worksheet mode and explicit history/reuse options. */
        public evaluateLabMode(mode: WorksheetMode, options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            const frame: MarsApiEvaluationFrame = {mode: this.dependencies.workspace.workspaceModeId(mode), options};
            return this.dependencies.flow.run(0, frame);
        }

        /** @brief Install an opaque native result using the supplied mode and evaluation context. */
        public installEvaluationResult(mode: number, data: unknown, context: MarsApiInstallContext = {},
                                         outcome = 2, request?: MarsApiRequest | null): Promise<unknown> {
            const frame: MarsApiInstallFrame = {mode, data, context, outcome, request};
            return this.dependencies.flow.continue(5, frame);
        }

        /** @brief Ask the native startup workflow whether the active worksheet needs evaluation. */
        public evaluateActiveModeOnLoad(): Promise<unknown> | undefined {
            return this.dependencies.flow.run(4, {});
        }

        /** @brief Evaluate the Expression worksheet with the shared native admission policy. */
        public evaluateExpression(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('expression', options);
        }

        /** @brief Evaluate the Equation worksheet with the shared native admission policy. */
        public evaluateEquation(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('equation', options);
        }

        /** @brief Evaluate the Diffequation worksheet with the shared native admission policy. */
        public evaluateDiffequation(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('diffequation', options);
        }

        /** @brief Evaluate the Matrix worksheet with the shared native admission policy. */
        public evaluateMatrix(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('matrix', options);
        }

        /** @brief Evaluate the Integrator worksheet with the shared native admission policy. */
        public evaluateIntegrator(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('integrator', options);
        }

        /** @brief Evaluate the Datetime worksheet with the shared native admission policy. */
        public evaluateDatetime(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('datetime', options);
        }

        /** @brief Evaluate the Almanac worksheet with the shared native admission policy. */
        public evaluateAlmanac(options: WorksheetEvaluationOptions = {}): Promise<unknown> | undefined {
            return this.evaluateLabMode('almanac', options);
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                currentMode: () => this.dependencies.workspace.currentMode(),
                expressionText: () => this.dependencies.bindings.currentExpressionText(),
                historyState: (text: string) => this.dependencies.persistence.historyStateForMode(this.dependencies.workspace.currentMode(), text),
                previousHistory: (state: WorksheetSnapshot) => this.dependencies.persistence.previousModeStateForHistory(state),
                pushHistory: (state: WorksheetSnapshot) => this.dependencies.workspace.pushExpressionHistory(state),
                showResults: () => this.dependencies.workspace.showResults(),
                commitModeState: () => this.dependencies.persistence.commitModeState(),
                updateHistoryButtons: () => this.dependencies.workspace.updateHistoryButtons(),
                installEvaluation: (mode: number, data: unknown, context?: MarsApiInstallContext,
                                    outcome?: number, request?: MarsApiRequest | null) =>
                    this.installEvaluationResult(mode, data, context, outcome, request),
                evaluate: (mode: WorksheetMode) => this.evaluateLabMode(mode),
                precision: () => this.dependencies.workspace.requestedValuePrecision(),
                captureBindings: (context: MarsApiEvaluationContext) => context.enteredBindings = this.dependencies.bindings.visibleBindingValues(),
                commitBindings: () => this.dependencies.bindings.commitVisibleBindingInputs(),
                captureDatetime: (context: MarsApiEvaluationContext) => {
                    context.state = this.dependencies.locations.currentDatetimeState();
                    context.text = this.dependencies.locations.datetimeSummaryText(context.state);
                },
                captureAlmanac: (context: MarsApiEvaluationContext) => context.text = this.dependencies.locations.almanacSummaryText(this.dependencies.locations.currentAlmanacState()),
                assemble: async (context: MarsApiEvaluationContext) => context.entered =
                    await this.dependencies.bindings.expressionWithVisibleBindings(context.editorText, context.enteredBindings)
            } satisfies MarsApiEvaluationServices;
        }

    }

    /** @brief Capture goal inputs and expose native goal-seeking workflows. */
    export class GoalSeekController {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiGoalSeekControllerDependencies) {
            marsBindCallbacks(this, ['startGoalSeekFromEvent', 'runGoalSeek', 'capabilities']);
        }

        /** @brief Capture the native goal-start plan and request binding commitment before solving. */
        public async startGoalSeekFromEvent(): Promise<void> {
            const goal = this.dependencies.native.call('lab_evaluation_goal_start', this.dependencies.workspace.workspaceModeId(this.dependencies.workspace.currentMode()), this.dependencies.bindings.currentExpressionText()) as {text: string; target: string} | null;
            if (goal)
                await this.runGoalSeek(goal.text, goal.target, {}, {commitBindings: true});
        }

        /** @brief Begin native goal seeking with explicit target, starting values and evaluation options. */
        public runGoalSeek(sourceText: string, target: string, start: Record<string, unknown> = {},
                             options: MarsApiGoalOptions = {}): Promise<unknown> | undefined {
            const frame: MarsApiGoalFrame = {sourceText, target, start, options};
            return this.dependencies.flow.run(1, frame);
        }

        /** @brief Expose atomic browser operations; native C selects their execution order. */
        public capabilities() {
            return {
                goalPrepare: (source: string, start?: MarsApiStartValues) =>
                    this.dependencies.bindings.goalSeekExpressionAndStarts(source, start),
                goalEditorBody: (text: string) => this.dependencies.bindings.expressionBodyForEditor(text),
                expressionWithBindings: (body: string, bindings: WorksheetBinding[]) =>
                    this.dependencies.bindings.expressionWithBindings(body, bindings),
                goalExpression: (text: string) => this.dependencies.bindings.expressionTextOrEmpty(text)
            } satisfies MarsApiGoalServices;
        }

    }

    /** @brief Compose native capabilities and execute admitted browser continuations. */
    export class NativeFlowRuntime {
        /** @brief Retain explicit capabilities without invoking definition-time dependencies. */
        public constructor(private readonly dependencies: MarsApiNativeFlowRuntimeDependencies) {
            marsBindCallbacks(this, ['bind', 'dispose', 'getRegistry', 'continue', 'run', 'capabilities']);
        }

        private registry: NativeServiceRegistry | undefined;
        private providers: readonly MarsApiCapabilityProvider[] | undefined;
        private disposed = false;
        private readonly lifetime = new AbortController();

        /** @brief Release this runtime's registry; do not tear down injected controllers. */
        public dispose(): void {
            this.disposed = true;
            this.lifetime.abort();
            this.registry = undefined;
            this.providers = undefined;
        }

        /** @brief Attach providers once; no provider executes until the first native workflow. */
        public bind(providers: readonly MarsApiCapabilityProvider[]): void {
            if (this.disposed) throw new Error('Native workflow runtime has been disposed');
            if (this.providers) throw new Error('Native workflow providers are already bound');
            this.providers = [...providers];
        }

        /** @brief Assemble one immutable registry, rejecting ambiguous native service names. */
        public getRegistry(): NativeServiceRegistry {
            if (this.disposed) throw new Error('Native workflow runtime has been disposed');
            if (this.registry) return this.registry;
            if (!this.providers) throw new Error('Native workflow providers have not been bound');
            const registry: Record<string, MarsTransportService> = Object.create(null);
            for (const provide of this.providers) {
                for (const [name, service] of Object.entries(provide())) {
                    if (Object.prototype.hasOwnProperty.call(registry, name))
                        throw new Error('Duplicate native workflow service: ' + name);
                    registry[name] = service;
                }
            }
            this.registry = Object.freeze(registry);
            return this.registry;
        }

        /** @brief Execute a native continuation with browser-owned promises and exceptions. */
        public continue<T = unknown>(kind: number, frame: object): Promise<T> {
            return this.dependencies.native.flow(kind, frame, this.dependencies.results.labEvaluationView, this.getRegistry(), this.lifetime.signal) as Promise<T>;
        }

        /** @brief Ask C for admission policy, then run its continuation under request ownership. */
        public run(kind: number, frame: object): Promise<unknown> | undefined {
            const plan = this.dependencies.native.call('lab_flow_begin', kind, frame) as MarsApiFlowPlan | null;
            if (!plan) return;
            const advance = (request: MarsApiRequest) => this.continue(kind, Object.assign(frame, {request}));
            return plan.operation ? this.dependencies.requests[plan.ui ? 'runUI' : 'run'](
                plan.operation, plan.mode, advance, plan.options) : this.continue(kind, frame);
        }

        /** @brief Compose own-property service groups under the unchanged native ABI names. */
        public capabilities() {
            return {
                ...this.requestServices(),
                ...this.nativeServices(),
                ...this.errorServices()
            };
        }

        /** @brief Bind request lifetime and transport services without capturing a stale request owner. */
        private requestServices() {
            return {
                requestBegin: this.requestBegin.bind(this),
                requestBeginCurrent: this.requestBeginCurrent.bind(this),
                requestFinish: this.requestFinish.bind(this),
                requestCancel: this.requestCancel.bind(this),
                requestLatestMain: this.requestLatestMain.bind(this),
                runRequestFlow: this.runRequestFlow.bind(this),
                requestCurrent: this.requestCurrent.bind(this),
                requestOutcome: this.requestOutcome.bind(this),
                requestInput: this.requestInput.bind(this),
                post: this.postRequest.bind(this),
                requestContext: this.requestContext.bind(this),
                modeChanged: this.modeChanged.bind(this)
            } satisfies MarsApiRequestServices;
        }

        /** @brief Bind native dispatch and opaque-value forwarding independently of request admission. */
        private nativeServices() {
            return {
                native: this.callNative.bind(this),
                effects: this.applyEffects.bind(this),
                awaitValue: this.awaitValue.bind(this)
            } satisfies MarsApiNativeServices;
        }

        /** @brief Bind cancellation and failure propagation without changing exception identity. */
        private errorServices() {
            return {
                abort: this.abortRequest.bind(this),
                fail: this.failRequest.bind(this),
                rethrow: this.rethrowError.bind(this)
            } satisfies MarsApiErrorServices;
        }

        private requestBegin(operation: string, mode?: string, options?: MarsApiOptions): MarsApiRequest | null {
            return this.dependencies.requests.begin(operation, mode, options);
        }

        private requestBeginCurrent(operation: string, options: MarsApiOptions): MarsApiRequest | null {
            return this.dependencies.requests.begin(operation, this.dependencies.workspace.currentMode(), options);
        }

        private requestFinish(request: MarsApiRequest | null | undefined): void {
            return this.dependencies.requests.finish(request);
        }

        private requestCancel(operation: string): void {
            return this.dependencies.requests.cancel(operation);
        }

        private requestLatestMain(): number {
            return this.dependencies.requests.latestMain();
        }

        private runRequestFlow(kind: number, frame: object, stage: number, operation: string,
                                 mode: WorksheetMode, options: MarsApiOptions): Promise<unknown> {
            return this.dependencies.requests.runUI(
                operation, mode, this.advanceRequest.bind(this, kind, frame, stage), options);
        }

        /** @brief Copy the captured inputs only after admission attaches the request and continuation stage. */
        private advanceRequest(kind: number, frame: object, stage: number, request: MarsApiRequest): Promise<unknown> {
            return this.continue(kind, {...frame, stage, request});
        }

        private requestCurrent(request: MarsApiRequest | null): boolean {
            return this.dependencies.requests.current(request);
        }

        private requestOutcome(request: MarsApiRequest | null, response: MarsApiResponseStatus | null,
                                data: MarsApiData | null, result?: boolean): number {
            return this.dependencies.requests.outcome(request, response, data, result);
        }

        private requestInput(request: MarsApiRequest, text: unknown, variable?: boolean): boolean {
            return this.dependencies.requests.input(request, text, variable);
        }

        private postRequest(request: MarsApiRequest | null, payload: unknown, fallback: string,
                             signal?: AbortSignal): Promise<MarsApiPostResult> {
            return this.dependencies.requests.post(request, payload, fallback, signal);
        }

        private requestContext(): number {
            return this.dependencies.requests.context();
        }

        private modeChanged(mode: WorksheetMode): void {
            return this.dependencies.requests.modeChanged(mode);
        }

        private callNative(entry: string, ...args: unknown[]): unknown {
            return this.dependencies.native.call(entry, ...args);
        }

        private applyEffects(plan: NativeServicePlan): unknown {
            return this.dependencies.native.services(plan, this.getRegistry());
        }

        private awaitValue(value: unknown): unknown {
            return value;
        }

        private abortRequest(message: string): never {
            throw new DOMException(message, 'AbortError');
        }

        private failRequest(message: string): never {
            throw new Error(message);
        }

        private rethrowError(error: unknown): never {
            throw error;
        }

    }

    /** @brief Shared native request ownership and browser transport resources. */
    export const labRequests: LabRequestManager = new LabRequestManager({
        exports: () => labWire.exports(),
        mode: () => WorksheetWorkspace.mode.currentMode(),
        busy: value => WorksheetWorkspace.presentation.setBusy(value),
        running: (button, value) => WorksheetWorkspace.presentation.setActionRunning(button, value),
        invalidateSolver: () => labDOM.call('lab_result_solver_invalidate'),
        button: () => document.getElementById('run'),
        encode: value => labWire.encode(value),
        fetch: (url, options) => labHttp.fetch(url, options),
        get flow() { return labDOM.flow; },
        presentation: value => WorksheetResults.caches.install(value),
        syntax: value => WorksheetResults.caches.installSyntax(value),
        error: message => WorksheetResults.cards.setRenderedError(message),
        resetDigits: () => WorksheetResults.cards.resetMoreDigitsButton(WorksheetContext.renderedMore, false),
        clearResults: options => WorksheetResults.reset.clearResultDetails(options),
        status: message => WorksheetWorkspace.presentation.setStatus(message)
    });

    // Definition scripts load after api.ts. These small forwarding callbacks resolve
    // their current owners at invocation; only actual state and forward references need getters.

    /** @brief Application-owned presentation controller. */
    export const presentation: EditorPresentationController = new EditorPresentationController({
        results: {
            installLabPresentationData: (data) => WorksheetResults.caches.install(data)
        },
        get flow(): MarsApiFlowPort { return runtime; }
    });

    /** @brief Application-owned evaluationTransport controller. */
    export const evaluationTransport: EvaluationTransport = new EvaluationTransport({
        workspace: {
            currentMode: () => WorksheetWorkspace.mode.currentMode(),
            currentIntegratorBoundRows: (rows, expressionText) =>
                WorksheetWorkspace.integrator.activeIntegratorBoundRows(rows, expressionText),
            requestedValuePrecision: () => WorksheetWorkspace.precision.requestedValuePrecision(),
            currentIntegratorRows: () => WorksheetWorkspace.integrator.currentIntegratorRows(),
            integratorBoundsTextFromRows: (rows) => WorksheetWorkspace.integrator.forms.text(rows),
            refreshIntegratorForms: (rows, expression) => WorksheetWorkspace.integrator.refreshIntegratorForms(rows, expression),
            activeIntegratorRowPlan: (rows, expressionText) => WorksheetWorkspace.integrator.activeIntegratorRowPlan(rows, expressionText),
            requestedIntegratorIntervalCap: () => WorksheetWorkspace.integrator.requestedIntegratorIntervalCap()
        },
        bindings: {
            currentExpressionText: () => WorksheetBindings.editor.currentExpressionText(),
            renderIntegratorRows: (rows) => WorksheetBindings.integratorPanel.renderIntegratorRows(rows)
        },
        locations: {
            currentDatetimeState: () => WorksheetLocations.calendar.currentDatetimeState(),
            currentAlmanacState: () => WorksheetLocations.calendar.currentAlmanacState()
        },
        get context() { return WorksheetContext; },
        native: labDOM,
        http: labHttp,
        wire: labWire,
        get flow(): MarsApiFlowPort { return runtime; },
        requests: labRequests
    });

    /** @brief Application-owned functionCard controller. */
    export const functionCard: FunctionCardController = new FunctionCardController({
        workspace: {
            setActionRunning: (button, running) => WorksheetWorkspace.presentation.setActionRunning(button, running)
        },
        get context() { return WorksheetContext; },
        native: labDOM,
        get flow(): MarsApiFlowPort { return runtime; },
        requests: labRequests
    });

    /** @brief Application-owned plots controller. */
    export const plots: PlotRequestController = new PlotRequestController({
        workspace: {
            currentMode: () => WorksheetWorkspace.mode.currentMode(),
            variableNamesFromBindings: (bindings) => WorksheetWorkspace.bindingSelection.variableNamesFromBindings(bindings),
            renderDerivativeButtons: (variables) => WorksheetWorkspace.presentation.renderDerivativeButtons(variables)
        },
        bindings: {
            visibleBindingValues: () => WorksheetBindings.panel.visibleBindingValues(),
            expressionWithVisibleBindings: (sourceExpression, visibleBindings) =>
                WorksheetBindings.editor.expressionWithVisibleBindings(sourceExpression, visibleBindings)
        },
        results: {
            displayCalculusResult: (result) => WorksheetResults.cards.displayCalculusResult(result),
            setMatrixPrettyResult: (resultText, prettyText, element, moreButton) =>
                WorksheetResults.cards.setMatrixPrettyResult(resultText, prettyText, element, moreButton)
        },
        get context() { return WorksheetContext; },
        native: labDOM,
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        get bindingState() { return WorksheetBindings.panel; },
        get volume() { return labSurfaceVolume; },
        get preview() { return labSurfacePreview; },
        document,
        get flow(): MarsApiFlowPort { return runtime; },
        requests: labRequests
    });

    /** @brief Application-owned solver controller. */
    export const solver: SolverRenderController = new SolverRenderController({
        workspace: {
            currentMode: () => WorksheetWorkspace.mode.currentMode()
        },
        results: {
            scheduleSolverTexFit: (callback) => WorksheetResults.solverFit.schedule(callback)
        },
        native: labDOM,
        get flow(): MarsApiFlowPort { return runtime; },
        requests: labRequests
    });

    /** @brief Application-owned calendarRequests controller. */
    export const calendarRequests: CalendarRequestController = new CalendarRequestController({
        locations: {
            refreshDatetimeJurisdictionLocation: (options) =>
                WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation(options)
        },
        results: {
            bindAlmanacTotalityActions: (node) => WorksheetResults.calendarResults.bindAlmanacTotalityActions(node)
        },
        get context() { return WorksheetContext; },
        native: labDOM,
        http: labHttp,
        application: labApplication,
        get flow(): MarsApiFlowPort { return runtime; }
    });

    /** @brief Application-owned evaluation controller. */
    export const evaluation: EvaluationController = new EvaluationController({
        workspace: {
            workspaceModeId: (mode) => WorksheetWorkspace.mode.workspaceModeId(mode),
            currentMode: () => WorksheetWorkspace.mode.currentMode(),
            pushExpressionHistory: (entry) => WorksheetWorkspace.history.pushExpressionHistory(entry),
            showResults: () => WorksheetWorkspace.presentation.showResults(),
            updateHistoryButtons: () => WorksheetWorkspace.presentation.updateHistoryButtons(),
            requestedValuePrecision: () => WorksheetWorkspace.precision.requestedValuePrecision()
        },
        bindings: {
            currentExpressionText: () => WorksheetBindings.editor.currentExpressionText(),
            visibleBindingValues: () => WorksheetBindings.panel.visibleBindingValues(),
            commitVisibleBindingInputs: (isCurrent) => WorksheetBindings.controller.commitVisibleBindingInputs(isCurrent),
            expressionWithVisibleBindings: (sourceExpression, visibleBindings) =>
                WorksheetBindings.editor.expressionWithVisibleBindings(sourceExpression, visibleBindings)
        },
        locations: {
            currentDatetimeState: () => WorksheetLocations.calendar.currentDatetimeState(),
            datetimeSummaryText: (state) => WorksheetLocations.calendar.datetimeSummaryText(state),
            almanacSummaryText: (state) => WorksheetLocations.calendar.almanacSummaryText(state),
            currentAlmanacState: () => WorksheetLocations.calendar.currentAlmanacState()
        },
        persistence: {
            historyStateForMode: (mode, textOverride) => WorksheetPersistence.history.snapshot(mode, textOverride),
            previousModeStateForHistory: (nextState) => WorksheetPersistence.history.previous(nextState),
            commitModeState: (mode, textOverride) => WorksheetPersistence.history.commit(mode, textOverride)
        },
        get flow(): MarsApiFlowPort { return runtime; }
    });

    /** @brief Application-owned goals controller. */
    export const goals: GoalSeekController = new GoalSeekController({
        workspace: {
            workspaceModeId: (mode) => WorksheetWorkspace.mode.workspaceModeId(mode),
            currentMode: () => WorksheetWorkspace.mode.currentMode()
        },
        bindings: {
            currentExpressionText: () => WorksheetBindings.editor.currentExpressionText(),
            goalSeekExpressionAndStarts: (sourceExpression, providedStart) =>
                WorksheetBindings.goal.goalSeekExpressionAndStarts(sourceExpression, providedStart),
            expressionBodyForEditor: (text) => WorksheetBindings.editor.expressionBodyForEditor(text),
            expressionWithBindings: (bodyText, bindings) => WorksheetBindings.editor.expressionWithBindings(bodyText, bindings),
            expressionTextOrEmpty: (text) => WorksheetBindings.expressionTextOrEmpty(text)
        },
        native: labDOM,
        get flow(): MarsApiFlowPort { return runtime; }
    });

    /** @brief Application-owned runtime controller. */
    export const runtime: NativeFlowRuntime = new NativeFlowRuntime({
        workspace: {
            currentMode: () => WorksheetWorkspace.mode.currentMode()
        },
        results: {
            get labEvaluationView() { return WorksheetResults.labEvaluationView; }
        },
        native: labDOM,
        requests: labRequests
    });

    // Existing result providers share some ABI names. Select their established owner
    // explicitly before registration; the runtime rejects all remaining collisions.
    runtime.bind([
        () => {
            const {
                setRenderedError: _error, resetRenderedDigits: _digits, clearResultDetails: _clear,
                setDatetimeLocalText: _local, applyIntegratorBindingState: _bindings,
                applyIntegratorResultBound: _bound, saveWorksheetState: _save, ...services
            } = WorksheetResults.labEvaluationRecovery;
            return services;
        },
        () => {
            const {setRenderedResult: _render, ...services} = WorksheetResults.labEvaluationInstall;
            return services;
        },
        () => {
            const {setStatus: _status, ...services} = WorksheetResults.labWeatherServices;
            return services;
        },
        () => WorksheetResults.labGoalServices,
        () => ({...WorksheetPersistence.capabilities}),
        () => WorksheetLocations.locationServices,
        () => WorksheetBindings.controller.labBindingFlowServices(),
        () => WorksheetResults.resultServices,
        presentation.capabilities,
        evaluationTransport.capabilities,
        functionCard.capabilities,
        plots.capabilities,
        solver.capabilities,
        calendarRequests.capabilities,
        evaluation.capabilities,
        goals.capabilities,
        runtime.capabilities
    ]);

}
