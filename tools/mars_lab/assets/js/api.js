/**
 * Browser requests to native mathematical, calendar and mobile-access endpoints.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

// Browser transport only: C selects layouts and native request tokens reject stale replies.
let labSolverRenderRequest = null;

function fitSolverTexToCard() {
    solverFitFrame = 0;
    return labFlowContinue(14, {});
}

/** Host resources only; the C module owns tokens, channels, routing and lifecycle decisions. */
const labRequests = (() => {
    const modes =
        Object.freeze({expression: 0, equation: 1, diffequation: 2, matrix: 3, integrator: 4, datetime: 5, almanac: 6});
    const resources = new Map();
    const native = () => labWire.exports();
    const decoder = new TextDecoder('utf-8', {fatal: true});
    function catalogueText(kind, index) {
        const controller = native();
        return decoder.decode(new Uint8Array(
            controller.memory.buffer, controller.lab_request_text(kind, index),
            controller.lab_request_text_length(kind, index)));
    }
    const operationCount = native().lab_request_operation_count();
    const operations = new Map(Array.from({length: operationCount}, (_, index) => [catalogueText(0, index), index]));
    const mainChannel = native().lab_request_channel(operations.get('evaluate'));
    const busy = () => Boolean(native().lab_request_busy());
    function reap() {
        for (const [channel, request] of resources) {
            if (native().lab_request_live(channel, request.token))
                continue;
            clearTimeout(request.timer);
            request.controller.abort();
            if (request.button)
                setActionRunning(request.button, false);
            if (request.disableButton && request.button)
                request.button.disabled = false;
            resources.delete(channel);
        }
    }
    function modeChanged(mode) {
        native().lab_request_set_mode(modes[mode] ?? 7);
        reap();
        setBusy(busy());
    }
    function current(request) {
        modeChanged(currentMode());
        return Boolean(request && native().lab_request_live(request.channel, request.token));
    }
    function begin(operation, mode = currentMode(), options = {}) {
        modeChanged(currentMode());
        const action = operations.get(operation) ?? operationCount;
        const channel = native().lab_request_channel(action);
        const token = native().lab_request_begin(
                          modes[mode] ?? 7, action, options.input !== false, options.variable !== false,
                          Boolean(options.scalar), options.parent || 0) >>>
            0;
        if (!token)
            return null;
        const policy = native().lab_transport_policy(action, channel, mainChannel);
        if (policy & 2)
            labDOM.call('lab_result_solver_invalidate');
        reap();
        const request = {
            channel,
            token,
            controller: new AbortController(),
            timer: null,
            button: options.button || (policy & 1 ? document.getElementById('run') : null),
            disableButton: !!options.disableButton
        };
        resources.set(channel, request);
        if (request.button)
            setActionRunning(request.button, true);
        if (request.disableButton && request.button)
            request.button.disabled = true;
        const deadline = native().lab_request_deadline(channel, token);
        if (deadline)
            request.timer = setTimeout(() => {
                if (native().lab_request_timeout(channel, token))
                    request.controller.abort();
            }, deadline);
        setBusy(busy());
        return request;
    }
    function finish(request) {
        if (request)
            native().lab_request_finish(request.channel, request.token);
        reap();
        setBusy(busy());
    }
    function cancel(operation) {
        const channel = native().lab_request_channel(operations.get(operation) ?? operationCount);
        native().lab_request_cancel(channel);
        if (native().lab_transport_policy(operationCount, channel, mainChannel) & 2)
            labDOM.call('lab_result_solver_invalidate');
        reap();
        setBusy(busy());
    }
    function assertCurrent(request) {
        if (request && !current(request))
            throw new DOMException('Obsolete Lab request', 'AbortError');
        if (request && native().lab_request_timed_out(request.channel, request.token))
            throw new DOMException('Lab request timed out', 'AbortError');
    }
    function outcome(request, response, data, result = true) {
        if (!current(request))
            return 0;
        return native().lab_request_accept(
            request.channel, request.token, Boolean(response?.ok), Boolean(data?.ok), Boolean(result),
            Boolean(data?.partial_error));
    }
    const transportServices = Object.freeze({
        assertCurrent, current, finish,
        begin: frame => begin(frame.operation, frame.mode, frame.options),
        callback: frame => frame.callback(frame.request),
        endpoint: request => catalogueText(1, native().lab_request_endpoint(request.channel, request.token)),
        fetch: (endpoint, frame) => labFetch(endpoint, {
            method: 'POST', body: labWire.encode(frame.payload),
            signal: frame.request ? frame.request.controller.signal : frame.signal
        }),
        decode: response => response.labData(),
        presentation: frame => installLabPresentationData(frame.data),
        syntax: frame => installLabFunctionSyntaxData(frame.data),
        result: frame => ({response: frame.response, data: frame.data}),
        invalidEndpoint: () => { throw new Error('Invalid native request endpoint'); },
        rethrow: frame => { throw frame.exception; },
        renderError: error => setRenderedError(error),
        resetDigits: () => resetMoreDigitsButton(renderedMore, false),
        clearResults: options => clearResultDetails(options),
        status: text => setStatus(text)
    });
    function advance(kind, frame) {
        return labDOM.flow(kind, frame, null, transportServices).then(() => frame.result);
    }
    function post(request, payload, fallback, signal) {
        return advance(65, {request, payload, fallback, signal});
    }
    function run(operation, mode, callback, options = {}) {
        return advance(66, {operation, mode, callback, options});
    }
    function runUI(operation, mode, callback, options = {}) {
        return advance(67, {operation, mode, callback, options});
    }
    return {
        begin,
        current,
        finish,
        cancel,
        modeChanged,
        busy,
        post,
        run,
        runUI,
        outcome,
        context: () => native().lab_request_context() >>> 0,
        latestMain: () => native().lab_request_latest_main() >>> 0,
        action: request => catalogueText(2, native().lab_request_action(request.channel, request.token)),
        flags: request => native().lab_request_flags(request.channel, request.token),
        timedOut: request => Boolean(native().lab_request_timed_out(request.channel, request.token)),
        input: (request, text, variable = true) => current(request) &&
            Boolean(native().lab_request_input(request.channel, request.token, Boolean(text), Boolean(variable)))
    };
})();

/** Native text preparation; callers retain transport errors and can await edit boundaries. */
function requestLabForms(payload, options = {}) {
    return labFlowContinue(12, {payload, options});
}

const labPresentationEditorRequests = new Map();

function requestLabPresentation(payload, request = null) {
    return labFlowContinue(13, {payload, request});
}

function prepareLabEditor(text) {
    return labFlowContinue(13, {editor: true, text});
}

function clearFunctionRun() {
    labRequests.cancel('function');
    labDOM.call('lab_function_clear');
    setActionRunning(functionRun, false);
}

function runFunctionCard() {
    return labFlowContinue(6, {});
}

async function fetchEvaluation(text, wrt = '', action = '', bindingSource = '', bindingValue = '', request = null) {
    return labRequests.post(
        request,
        labDOM.call(
            'lab_payload_build', 0, [text, bindingSource, bindingValue, wrt, action, currentMode()], request,
            requestedValuePrecision(), lastExpressionUpdatedAt),
        '/eval');
}

function fetchMatrixEvaluation(options = {}) {
    return labFlowContinue(15, {mode: 0, options, request: options.request || null});
}

function fetchEquationEvaluation(request = null) {
    return labFlowContinue(15, {mode: 1, request});
}

function fetchDiffequationEvaluation(request = null) {
    return labFlowContinue(15, {mode: 2, request});
}

function fetchIntegratorEvaluation(request = null) {
    return labFlowContinue(8, {request});
}

function fetchDatetimeEvaluation(request = null) {
    return labFlowContinue(15, {mode: 3, request});
}

async function fetchDatetimeWeather(state, signal, request = null) {
    return labRequests.post(
        request, labDOM.call('lab_payload_build', 6, [state], request, 0, 0), '/datetime-weather', signal);
}

// Native continuations own calculus decisions; browser callbacks expose atomic capabilities.
function takeCalculus(wrt, action, actionButton = null) {
    return labFlowContinue(7, {wrt, action, actionButton});
}

async function takeDerivative(wrt, actionButton = null) {
    return takeCalculus(wrt, 'derivative', actionButton);
}

async function takeIntegral(wrt, actionButton = null) {
    return takeCalculus(wrt, 'integral', actionButton);
}

function fetchAlmanacEvaluation(request = null) {
    return labFlowContinue(15, {mode: 4, request});
}

function refreshMobileAccess() {
    return labFlowContinue(11, {});
}

function refreshAlmanacLandTotality(data) {
    return labFlowContinue(9, {data});
}

function refreshDatetimeLocalHolidays() {
    return labFlowContinue(10, {});
}

/** Browser resources and atomic application access; native continuations select their order. */
function labRequestFlowServices() {
    const fetchPayloads = [
        frame => labDOM.call(
            'lab_payload_build', 1, [frame.options, currentExpressionText(), frame.options.operand !== undefined],
            frame.request, requestedValuePrecision(), 0),
        frame =>
            labDOM.call('lab_payload_build', 2, [currentExpressionText()], frame.request, requestedValuePrecision(), 0),
        frame =>
            labDOM.call('lab_payload_build', 3, [currentExpressionText()], frame.request, requestedValuePrecision(), 0),
        () => currentDatetimeState(), () => currentAlmanacState()
    ];
    const solverOwner = () => labDOM.call('lab_result_solver_owner');
    return {
        requestBegin: (...args) => labRequests.begin(...args),
        requestBeginCurrent: (operation, options) => labRequests.begin(operation, currentMode(), options),
        requestFinish: request => labRequests.finish(request),
        requestCancel: operation => labRequests.cancel(operation),
        requestLatestMain: () => labRequests.latestMain(),
        runRequestFlow: (kind, frame, stage, operation, mode, options) =>
            labRequests.runUI(operation, mode, request => labFlowContinue(kind, {...frame, stage, request}), options),
        rawPost: (endpoint, payload, signal) =>
            labFetch(endpoint, {method: 'POST', body: labWire.encode(payload), signal}),
        responseData: response => response.labData(),
        installPresentation: data => installLabPresentationData(data),
        editorRequests: () => labPresentationEditorRequests,
        awaitValue: value => value,
        editorPromise: (source, pending, payload) => {
            pending.promise = requestLabPresentation(payload).then(data => data.editor).finally(() => {
                if (labPresentationEditorRequests.get(source) === pending)
                    labPresentationEditorRequests.delete(source);
            });
            labPresentationEditorRequests.set(source, pending);
            return pending.promise;
        },
        abort: message => {
            throw new DOMException(message, 'AbortError');
        },
        mobileFetch: () => labFetch(
            '/mobile-access', {cache: 'no-store', headers: controlToken ? {'X-Dval-Lab-Control': controlToken} : {}}),
        clearFunctionRun: () => clearFunctionRun(),
        buildPayload: (kind, inputs, request) =>
            labDOM.call('lab_payload_build', kind, inputs, request, requestedValuePrecision(), 0),
        functionComplete: (request, outcome, data) =>
            labDOM.call('lab_function_complete', outcome, Number(labRequests.timedOut(request)), data, null),
        functionException: (request, error) =>
            labDOM.call('lab_function_complete', 4, Number(labRequests.timedOut(request)), null, {
                get message() {
                    return String(error.message);
                }
            }),
        integratorSnapshot: () => {
            const text = currentExpressionText() || expr.value.trim();
            const rows = currentIntegratorRows();
            const context = labRequests.context();
            const rowsText = integratorBoundsTextFromRows(rows);
            return {text, rows, context, rowsText};
        },
        integratorText: () => currentExpressionText() || expr.value.trim(),
        integratorRowsText: () => integratorBoundsTextFromRows(currentIntegratorRows()),
        refreshIntegratorForms: (...args) => refreshIntegratorForms(...args),
        activeIntegratorRows: (...args) => activeIntegratorRowPlan(...args),
        renderIntegratorRows: rows => renderIntegratorRows(rows),
        integratorPayload: (text, bounds, request) => labDOM.call(
            'lab_payload_build', 4, [text, bounds, requestedIntegratorIntervalCap()], request,
            requestedValuePrecision(), 0),
        fetchPayload: frame => fetchPayloads[frame.mode](frame),
        landPayload: (data, cells) => labDOM.call('lab_payload_land', data, cells, labConfig),
        bindAlmanacTotalityActions: node => bindAlmanacTotalityActions(node),
        calculusInitial: () => ({mode: currentMode(), scalar: lastMatrixScalarExpression}),
        calculusCapture: () => ({
            bindings: visibleBindingValues(),
            variables: [...currentVariables],
            differentiable: currentDifferentiable
        }),
        matrixScalar: () => lastMatrixScalarExpression,
        expressionWithVisibleBindings: (...args) => expressionWithVisibleBindings(...args),
        calculusTitle: text => rightPaneTitle.textContent = text,
        valueTitle: text => valueTitle.textContent = text,
        requestPresentation: (...args) => requestLabPresentation(...args),
        fetchCalculus: (text, wrt, action, request) => fetchEvaluation(text, wrt, action, '', '', request),
        fetchMatrixOptions: options => fetchMatrixEvaluation(options),
        displayCalculusResult: result => displayCalculusResult(result),
        setMatrixPrettyResult: (...args) => setMatrixPrettyResult(...args),
        calculusVariables: variables => currentVariables = variables,
        calculusBindingVariables: bindings => currentVariables = variableNamesFromBindings(bindings || []),
        calculusDifferentiable: enabled => currentDifferentiable = enabled,
        calculusButtons: () => renderDerivativeButtons(currentVariables),
        solverPrepare: () => labDOM.call(
            'lab_solver_view_prepare', currentMode(), labSolverRenderRequest,
            Number(labSolverRenderRequest && labRequests.current(labSolverRenderRequest.request)), solverOwner()),
        solverPending: (pending, request) => {
            pending.request = request;
            labSolverRenderRequest = pending;
            pending.current = () => !!labDOM.call(
                'lab_solver_view_current', currentMode(), pending, labSolverRenderRequest,
                Number(labRequests.current(request)), solverOwner());
        },
        solverPublish: (pending, response, data, failed) => labDOM.call(
            'lab_solver_view_publish', currentMode(), pending, labSolverRenderRequest,
            Number(labRequests.current(pending.request)), solverOwner(), response, data, failed),
        solverRelease: pending => {
            if (labSolverRenderRequest === pending)
                labSolverRenderRequest = null;
        }
    };
}

/** Execute native continuations; promises and exception objects remain browser-owned. */
function labFlowContinue(kind, frame) {
    return labDOM.flow(kind, frame, labEvaluationView, labFlowCapabilities());
}

function labFlowRun(kind, frame) {
    const plan = labDOM.call('lab_flow_begin', kind, frame);
    if (!plan)
        return;
    const advance = request => labFlowContinue(kind, Object.assign(frame, {request}));
    return plan.operation ? labRequests[plan.ui ? 'runUI' : 'run'](plan.operation, plan.mode, advance, plan.options) :
                            labFlowContinue(kind, frame);
}

const labEvaluationSetup = Object.freeze({
    captureBindings: context => context.enteredBindings = visibleBindingValues(),
    commitBindings: () => commitVisibleBindingInputs(),
    captureDatetime: context => {
        context.state = currentDatetimeState();
        context.text = datetimeSummaryText(context.state);
    },
    captureAlmanac: context => context.text = almanacSummaryText(currentAlmanacState()),
    assemble: async context => context.entered =
        await expressionWithVisibleBindings(context.editorText, context.enteredBindings),
    saveWorksheetState: (...args) => saveWorksheetState(...args)
});


// Resolve cross-script capabilities after definition-only modules have all loaded.
let labFlowRegistry;
function labFlowCapabilities() {
    return labFlowRegistry ||= Object.freeze({
        ...labEvaluationRecovery,
        ...labEvaluationSetup,
        ...labEvaluationInstall,
        ...labWeatherServices,
        ...labGoalServices,
        ...labStateFlowServices(),
        ...labLocationFlowServices(),
        ...labBindingFlowServices(),
        ...labResultFlowServices(),
        ...labRequestFlowServices(),
        native: (entry, ...args) => labDOM.call(entry, ...args),
        effects: plan => labDOM.services(plan, labFlowCapabilities()),
        requestCurrent: request => labRequests.current(request),
        requestOutcome: (...args) => labRequests.outcome(...args),
        requestInput: (...args) => labRequests.input(...args),
        post: (...args) => labRequests.post(...args),
        requestContext: () => labRequests.context(),
        modeChanged: mode => labRequests.modeChanged(mode),
        currentMode: () => currentMode(),
        expressionText: () => currentExpressionText(),
        historyState: text => historyStateForMode(currentMode(), text),
        previousHistory: state => previousModeStateForHistory(state),
        pushHistory: state => pushExpressionHistory(state),
        showResults: () => showResults(),
        commitModeState: () => commitModeState(),
        updateHistoryButtons: () => updateHistoryButtons(),
        fetchExpression: (context, request) => fetchEvaluation(context.text, '', '', '', '', request),
        fetchEquation: (_context, request) => fetchEquationEvaluation(request),
        fetchDiffequation: (_context, request) => fetchDiffequationEvaluation(request),
        fetchMatrix: (_context, request) => fetchMatrixEvaluation({request}),
        fetchIntegrator: (_context, request) => fetchIntegratorEvaluation(request),
        fetchDatetime: (_context, request) => fetchDatetimeEvaluation(request),
        fetchAlmanac: (_context, request) => fetchAlmanacEvaluation(request),
        fetchWeather: (state, request) => fetchDatetimeWeather(state, undefined, request),
        installEvaluation: (...args) => installEvaluationResult(...args),
        renderSolver: (source, parent) => labFlowRun(3, {source, parent}),
        solverSVG: (...args) => {
            labDOM.call('lab_evaluation_solver', ...args);
            scheduleSolverTexFit();
        },
        refreshWeather: (...args) => {
            void refreshDatetimeWeather(...args);
        },
        refreshLocation: () => refreshDatetimeJurisdictionLocation(),
        evaluate: mode => evaluateLabMode(mode),
        goalPrepare: (...args) => goalSeekExpressionAndStarts(...args),
        goalEditorBody: text => expressionBodyForEditor(text),
        expressionWithBindings: (...args) => expressionWithBindings(...args),
        goalExpression: text => expressionForEvaluation(text),
        precision: () => requestedValuePrecision(),
        fail: message => {
            throw new Error(message);
        },
        rethrow: error => {
            throw error;
        }
    });
}

function evaluateLabMode(mode, options = {}) {
    return labFlowRun(0, {mode: workspaceModeId(mode), options});
}

function installEvaluationResult(mode, data, context = {}, outcome = 2, request) {
    return labFlowContinue(5, {mode, data, context, outcome, request});
}

function evaluateExpression(options = {}) {
    return evaluateLabMode('expression', options);
}
function evaluateEquation(options = {}) {
    return evaluateLabMode('equation', options);
}
function evaluateDiffequation(options = {}) {
    return evaluateLabMode('diffequation', options);
}
function evaluateMatrix(options = {}) {
    return evaluateLabMode('matrix', options);
}
function evaluateIntegrator(options = {}) {
    return evaluateLabMode('integrator', options);
}
function evaluateDatetime(options = {}) {
    return evaluateLabMode('datetime', options);
}
function evaluateAlmanac(options = {}) {
    return evaluateLabMode('almanac', options);
}

function refreshDatetimeWeather(parent, state, overviewData) {
    return labFlowRun(2, {parent, state, overviewData});
}

function evaluateActiveModeOnLoad() {
    return labFlowRun(4, {});
}

async function startGoalSeekFromEvent() {
    const goal = labDOM.call('lab_evaluation_goal_start', workspaceModeId(currentMode()), currentExpressionText());
    if (goal)
        await runGoalSeek(goal.text, goal.target, {}, {commitBindings: true});
}

function runGoalSeek(sourceText, target, start = {}, options = {}) {
    return labFlowRun(1, {sourceText, target, start, options});
}
