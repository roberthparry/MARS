/**
 * @file function_checks.ts
 * @brief Function-card projection and execution-ownership regressions.
 *
 * Controls browser requests and native presentation to verify asynchronous function execution while preserving surrounding card state.
 */

/// <reference path="./function_checks.d.ts" />
/** @brief Check function-card native projection and asynchronous execution ownership. */
window.checkLabFunction = async function checkLabFunction() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Function: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const native = labWire.exports();
    const savedMode = WorksheetWorkspace.mode.currentMode();
    const savedFetch = window.fetch;
    const savedPost = WorksheetRequests.labRequests.post;
    const nodes = [
        WorksheetContext.functionTitle, WorksheetContext.functionStyle, WorksheetContext.functionRun,
        WorksheetContext.functionRunResult, WorksheetContext.functionRunOutput
    ];
    const savedNodes: LabFunctionNodeSnapshot[] = [];
    for (const node of nodes) {
        const children = [...node.childNodes];
        const attributes: LabFunctionAttribute[] = [];
        for (const attribute of node.attributes) {
            attributes.push({name: attribute.name, value: attribute.value});
        }
        savedNodes.push({node, children, attributes});
    }
    const originals = new Map<string, unknown>();
    const requests: (WorksheetRequests.MarsApiRequest | null | undefined)[] = [];
    const pending: LabFunctionPendingRequest[] = [];
    const events: [unknown, unknown][] = [];
    /** @brief Replace a fixture service while retaining its first implementation for cleanup. */
    function replace(name: string, callback: unknown): void {
        if (!originals.has(name))
            originals.set(name, Reflect.get(labTestModuleOwner(name), name));
        Reflect.set(labTestModuleOwner(name), name, callback);
    }

    /** @brief Encode a fixture reply using the production wire format. */
    function response(data: unknown, status = 200): Response {
        return new Response(labWire.encode(data), {status, headers: {'Content-Type': 'application/x-protobuf'}});
    }

    /** @brief Project a completion through the native function-card policy. */
    function complete(outcome: number, data: unknown = {}, timeout = false, error: unknown = null) {
        return labDOM.call('lab_function_complete', outcome, Number(timeout), data, error);
    }

    /** @brief Defer browser message conversion until the native policy reads it. */
    function messageView(error: {message: unknown}) {
        return {
            get message() {
                return String(error.message);
            }
        };
    }

    /** @brief Reject a wrapper that completes before entering its fetch adapter. */
    function requestDidNotStart(): never {
        throw new Error('Function: request was not started');
    }
    let mode = 'expression';
    try {
        WorksheetContext.functionRun.disabled = false;
        WorksheetContext.functionTitle.textContent = 'Function';
        WorksheetContext.functionStyle.dataset.fullText = '  exact source μ\u0000β  ';
        equal(labDOM.call('lab_function_source'), 'exact source μ\u0000β', 'full source remains opaque');
        WorksheetContext.functionRun.disabled = true;
        equal(labDOM.call('lab_function_source'), null, 'disabled RUN cannot supply a source');
        WorksheetContext.functionRun.disabled = false;
        for (const title of ['', 'Function ', 'function', 'Derivation']) {
            WorksheetContext.functionTitle.textContent = title;
            equal(labDOM.call('lab_function_source'), null, 'title must match exactly');
        }
        WorksheetContext.functionTitle.textContent = 'Function';
        for (const source of ['', ' \n\t ']) {
            WorksheetContext.functionStyle.dataset.fullText = source;
            equal(labDOM.call('lab_function_source'), '', 'empty eligible source differs from null');
        }
        delete WorksheetContext.functionStyle.dataset.fullText;
        equal(labDOM.call('lab_function_source'), '', 'missing full source becomes empty text');
        WorksheetContext.functionRun.disabled = true;
        WorksheetContext.functionRunOutput.textContent = 'old output';
        WorksheetContext.functionRunResult.classList.remove('hidden');
        labDOM.call('lab_function_clear');
        equal(WorksheetContext.functionRun.disabled, false, 'clear enables RUN');
        equal(WorksheetContext.functionRunOutput.textContent, '', 'clear removes old output');
        check(WorksheetContext.functionRunResult.classList.contains('hidden'), 'clear hides execution results');
        labDOM.call('lab_function_start', 0);
        check(!WorksheetContext.functionRunResult.classList.contains('hidden'), 'unavailable programme still shows its diagnostic');
        equal(
            WorksheetContext.functionRunOutput.textContent, 'No Function programme is available. Evaluate an input first.',
            'unavailable diagnostic');
        equal(WorksheetContext.functionRun.disabled, false, 'unavailable does not disable RUN');
        labDOM.call('lab_function_start', 1);
        equal(WorksheetContext.functionRunOutput.textContent, 'Running…', 'running text');
        equal(WorksheetContext.functionRun.disabled, true, 'running disables RUN');

        const unread = {
            get output() {
                throw new Error('unexpected output read');
            }
        };
        for (const outcome of [0, 3, -1, 5, 2147483647]) {
            complete(outcome, unread);
            equal(WorksheetContext.functionRunOutput.textContent, 'Running…', 'invalid or stale completion cannot change output');
        }
        for (const output of [undefined, null, false, 0, '', ' \n\u00a0', '  indented μ\n\t', '\u200b']) {
            complete(2, {output, error: 'ignored'});
            equal(
                WorksheetContext.functionRunOutput.textContent, String(output || '').trimEnd() || 'Programme completed without output.',
                'success trims trailing whitespace only');
        }
        complete(2, {output: '1e-5\n', presentation: {output_display: '1×10⁻⁵\n'}});
        equal(WorksheetContext.functionRunOutput.textContent, '1×10⁻⁵', 'native numerical typography is displayed without parsing');
        for (const output of ['', ' \n', '  partial μ\n']) {
            for (const error of ['', ' \t ', ' native diagnostic \n']) {
                complete(1, {output, error});
                const partialOutput = String(output || '').trimEnd();
                const diagnostic = String(error || '').trim() || 'Programme execution failed.';
                let expected = diagnostic;
                if (partialOutput) {
                    expected = partialOutput + '\n\n' + diagnostic;
                }
                equal(
                    WorksheetContext.functionRunOutput.textContent, expected,
                    'failed output and diagnostic composition');
            }
        }
        for (const outcome of [1, 2]) {
            for (const timeout of [false, true]) {
                const reads: string[] = [];
                complete(
                    outcome, {
                        get output() {
                            reads.push('output');
                            return '  text\n';
                        },
                        get error() {
                            reads.push('error');
                            return ' diagnostic ';
                        }
                    },
                    timeout);
                equal(reads.join(','), 'output,error', 'both fields read in order, even on timeout or success');
                let expected = '  text\n\ndiagnostic';
                if (outcome === 2) {
                    expected = '  text';
                } else if (timeout) {
                    expected = 'Execution request timed out.';
                }
                equal(
                    WorksheetContext.functionRunOutput.textContent, expected,
                    'timeout precedence');
            }
        }
        for (const message of [undefined, null, '', 0, false, 'detail μ']) {
            complete(4, unread, false, messageView({message}));
            equal(
                WorksheetContext.functionRunOutput.textContent, 'Could not run programme: ' + String(message),
                'exception view preserves browser message conversion');
        }
        complete(4, unread, true, {
            get message() {
                throw new Error('timeout must not read message');
            }
        });
        equal(WorksheetContext.functionRunOutput.textContent, 'Execution request timed out.', 'exception timeout skips message getter');
        equal(WorksheetContext.functionRun.disabled, true, 'completion never re-enables the button');

        replace('currentMode', () => mode);
        replace('setBusy', () => {});
        replace('setActionRunning', (button: unknown, running: unknown) => events.push([button, running]));
        WorksheetRequests.labRequests.post = function captureRequest(request, ...args) {
            requests.push(request);
            return savedPost(request, ...args);
        };
        WorksheetRequests.labRequests.modeChanged(mode);
        WorksheetContext.functionTitle.textContent = 'Function';
        WorksheetContext.functionStyle.dataset.fullText = ' \nexact full programme μ\n ';
        const source = WorksheetContext.functionStyle.dataset.fullText.trim();
        let fetches = 0;
        window.fetch = function rejectIneligibleFetch() {
            ++fetches;
            throw new Error('ineligible card cannot fetch');
        };
        WorksheetContext.functionRun.disabled = true;
        await WorksheetRequests.functionCard.runFunctionCard();
        WorksheetContext.functionRun.disabled = false;
        WorksheetContext.functionTitle.textContent = 'Not Function';
        await WorksheetRequests.functionCard.runFunctionCard();
        WorksheetContext.functionTitle.textContent = 'Function';
        WorksheetContext.functionStyle.dataset.fullText = '  ';
        await WorksheetRequests.functionCard.runFunctionCard();
        equal(fetches, 0, 'disabled, wrong-title and empty-source cases never fetch');
        equal(
            WorksheetContext.functionRunOutput.textContent, 'No Function programme is available. Evaluate an input first.',
            'empty eligible source reaches availability projection');
        WorksheetContext.functionStyle.dataset.fullText = source;

        for (const scenario
                 of ['success', 'empty success', 'native failure', 'HTTP failure', 'network failure',
                     'undefined message', 'null message', 'timeout response', 'timeout catch', 'stale success',
                     'stale failure', 'stale catch']) {
            const gate = LabTestBrowser.responseGate();
            const entered = LabTestBrowser.signal();
            let signal: AbortSignal | null = null;
            WorksheetContext.functionRun.disabled = false;
            WorksheetContext.functionTitle.textContent = 'Function';
            window.fetch = function captureFunctionFetch(url, options) {
                entered.resolve();
                ++fetches;
                equal(String(url), '/function-run', 'native programme endpoint');
                if (!(options?.body instanceof Uint8Array) || !options.signal) {
                    throw new Error('Missing binary request or signal');
                }
                const payload = labWire.decode(options.body as Uint8Array<ArrayBuffer>) as {source: string};
                equal(payload.source, source, 'executes full source, not abbreviated visible text');
                signal = options.signal;
                return gate.promise;
            };
            const task = WorksheetRequests.functionCard.runFunctionCard();
            pending.push({task, gate});
            await Promise.race([entered.promise, task.then(requestDidNotStart)]);
            const request = requests.at(-1);
            check(request, 'real wrapper creates a native request');
            if (!request) {
                throw new Error('Missing native request');
            }
            equal(WorksheetContext.functionRunOutput.textContent, 'Running…', 'real wrapper projects running status');
            equal(WorksheetContext.functionRun.disabled, true, 'real wrapper disables button during request');
            check(!WorksheetContext.functionRunResult.classList.contains('hidden'), 'real wrapper reveals output');
            const stale = scenario.startsWith('stale');
            let newer = null;
            if (stale) {
                newer = WorksheetRequests.labRequests.begin('evaluate', mode);
                requests.push(newer);
                check(newer && (signal as AbortSignal | null)?.aborted, 'replacement evaluation cancels programme request');
                WorksheetContext.functionRunOutput.textContent = 'newer output';
                WorksheetContext.functionRun.disabled = true;
            } else if (scenario.startsWith('timeout')) {
                check(native.lab_request_timeout(request.channel, request.token), 'native timeout records ownership');
            }
            if (scenario === 'undefined message' || scenario === 'null message') {
                gate.reject({message: scenario === 'null message' ? null : undefined});
            } else if (scenario.endsWith('catch') || scenario === 'network failure') {
                gate.reject(new Error('network fixture'));
            } else {
                gate.resolve(response(
                    {
                        ok: scenario !== 'native failure' && scenario !== 'stale failure',
                        output: scenario === 'empty success' ? ' \n' : '  native output μ\n',
                        error: ' native failure detail '
                    },
                    scenario === 'HTTP failure' ? 503 : 200));
            }
            await task;
            if (stale) {
                equal(WorksheetContext.functionRunOutput.textContent, 'newer output', 'stale completion preserves newer output');
                equal(WorksheetContext.functionRun.disabled, true, 'stale finaliser cannot enable a newer button');
                check(WorksheetRequests.labRequests.busy(), 'stale finaliser preserves replacement evaluation ownership');
            } else {
                const expected: LabFunctionExpectedOutputs = {
                    success: '  native output μ',
                    'empty success': 'Programme completed without output.',
                    'native failure': '  native output μ\n\nnative failure detail',
                    'HTTP failure': '  native output μ\n\nnative failure detail',
                    'network failure': 'Could not run programme: network fixture',
                    'undefined message': 'Could not run programme: undefined',
                    'null message': 'Could not run programme: null',
                    'timeout response': 'Execution request timed out.',
                    'timeout catch': 'Execution request timed out.'
                };
                equal(WorksheetContext.functionRunOutput.textContent, expected[scenario], 'real wrapper completion: ' + scenario);
                equal(WorksheetContext.functionRun.disabled, false, 'current completion re-enables RUN');
                check(!WorksheetRequests.labRequests.current(request), 'completed request releases native ownership');
            }
            if (newer)
                WorksheetRequests.labRequests.finish(newer);
        }
        // A diagnostic getter may itself fail; the native continuation must still run the original finaliser.
        const diagnosticFailure = new Error('diagnostic getter failure');
        window.fetch = async function rejectDiagnosticFetch() {
            throw {
                get message() {
                    throw diagnosticFailure;
                }
            };
        };
        WorksheetContext.functionRun.disabled = false;
        let propagated;
        try {
            await WorksheetRequests.functionCard.runFunctionCard();
        } catch (caught) {
            propagated = caught;
        }
        equal(propagated, diagnosticFailure, 'diagnostic getter failure propagates unchanged');
        equal(WorksheetContext.functionRun.disabled, false, 'diagnostic getter failure still enables RUN');
        check(!WorksheetRequests.labRequests.current(requests.at(-1)), 'diagnostic getter failure releases ownership');

        check(events.some(([button, running]) => button === WorksheetContext.functionRun && running), 'host starts RUN indicator');
        check(events.some(([button, running]) => button === WorksheetContext.functionRun && !running), 'host clears RUN indicator');
    } finally {
        for (const item of pending) {
            item.gate.resolve(response({ok: false, error: 'fixture cleanup'}));
            try {
                await item.task;
            } catch (_) {
                // Preserve the original assertion while releasing suspended fixtures.
            }
        }
        for (const request of requests) {
            if (request) {
                WorksheetRequests.labRequests.finish(request);
            }
        }
        window.fetch = savedFetch;
        WorksheetRequests.labRequests.post = savedPost;
        for (const [name, callback] of originals) {
            Reflect.set(labTestModuleOwner(name), name, callback);
        }
        WorksheetRequests.labRequests.modeChanged(savedMode);
        for (const {node, children, attributes} of savedNodes) {
            for (const attribute of [...node.attributes]) {
                node.removeAttribute(attribute.name);
            }
            for (const attribute of attributes) {
                node.setAttribute(attribute.name, attribute.value);
            }
            node.replaceChildren(...children);
        }
    }
};
