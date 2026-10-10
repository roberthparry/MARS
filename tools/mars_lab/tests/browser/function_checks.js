/** Function-card native projection and asynchronous execution ownership regressions. */
window.checkLabFunction = async function checkLabFunction() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Function: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const native = labWire.exports(), savedMode = currentMode();
    const savedFetch = window.fetch, savedPost = labRequests.post;
    const nodes = [functionTitle, functionStyle, functionRun, functionRunResult, functionRunOutput];
    const savedNodes = nodes.map(
        node => ({node, children: [...node.childNodes], attributes: [...node.attributes].map(a => [a.name, a.value])}));
    const originals = new Map(), requests = [], pending = [], events = [];
    const replace = (name, callback) => {
        if (!originals.has(name))
            originals.set(name, window[name]);
        window[name] = callback;
    };
    const deferred = () => {
        let resolve, reject;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const response = (data, status = 200) =>
        new Response(labWire.encode(data), {status, headers: {'Content-Type': 'application/x-protobuf'}});
    const complete = (outcome, data = {}, timeout = false, error = null) =>
        labDOM.call('lab_function_complete', outcome, Number(timeout), data, error);
    const messageView = error => ({
        get message() {
            return String(error.message);
        }
    });
    let mode = 'expression';
    try {
        functionRun.disabled = false;
        functionTitle.textContent = 'Function';
        functionStyle.dataset.fullText = '  exact source μ\u0000β  ';
        equal(labDOM.call('lab_function_source'), 'exact source μ\u0000β', 'full source remains opaque');
        functionRun.disabled = true;
        equal(labDOM.call('lab_function_source'), null, 'disabled RUN cannot supply a source');
        functionRun.disabled = false;
        for (const title of ['', 'Function ', 'function', 'Derivation']) {
            functionTitle.textContent = title;
            equal(labDOM.call('lab_function_source'), null, 'title must match exactly');
        }
        functionTitle.textContent = 'Function';
        for (const source of ['', ' \n\t ']) {
            functionStyle.dataset.fullText = source;
            equal(labDOM.call('lab_function_source'), '', 'empty eligible source differs from null');
        }
        delete functionStyle.dataset.fullText;
        equal(labDOM.call('lab_function_source'), '', 'missing full source becomes empty text');
        functionRun.disabled = true;
        functionRunOutput.textContent = 'old output';
        functionRunResult.classList.remove('hidden');
        labDOM.call('lab_function_clear');
        equal(functionRun.disabled, false, 'clear enables RUN');
        equal(functionRunOutput.textContent, '', 'clear removes old output');
        check(functionRunResult.classList.contains('hidden'), 'clear hides execution results');
        labDOM.call('lab_function_start', 0);
        check(!functionRunResult.classList.contains('hidden'), 'unavailable programme still shows its diagnostic');
        equal(
            functionRunOutput.textContent, 'No Function programme is available. Evaluate an input first.',
            'unavailable diagnostic');
        equal(functionRun.disabled, false, 'unavailable does not disable RUN');
        labDOM.call('lab_function_start', 1);
        equal(functionRunOutput.textContent, 'Running…', 'running text');
        equal(functionRun.disabled, true, 'running disables RUN');

        const unread = {
            get output() {
                throw new Error('unexpected output read');
            }
        };
        for (const outcome of [0, 3, -1, 5, 2147483647]) {
            complete(outcome, unread);
            equal(functionRunOutput.textContent, 'Running…', 'invalid or stale completion cannot change output');
        }
        for (const output of [undefined, null, false, 0, '', ' \n\u00a0', '  indented μ\n\t', '\u200b']) {
            complete(2, {output, error: 'ignored'});
            equal(
                functionRunOutput.textContent, String(output || '').trimEnd() || 'Programme completed without output.',
                'success trims trailing whitespace only');
        }
        for (const output of ['', ' \n', '  partial μ\n']) {
            for (const error of ['', ' \t ', ' native diagnostic \n']) {
                complete(1, {output, error});
                equal(
                    functionRunOutput.textContent,
                    [String(output || '').trimEnd(), String(error || '').trim() || 'Programme execution failed.']
                        .filter(Boolean)
                        .join('\n\n'),
                    'failed output and diagnostic composition');
            }
        }
        for (const outcome of [1, 2]) {
            for (const timeout of [false, true]) {
                const reads = [];
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
                equal(
                    functionRunOutput.textContent,
                    outcome === 2 ? '  text' :
                        timeout   ? 'Execution request timed out.' :
                                    '  text\n\ndiagnostic',
                    'timeout precedence');
            }
        }
        for (const message of [undefined, null, '', 0, false, 'detail μ']) {
            complete(4, unread, false, messageView({message}));
            equal(
                functionRunOutput.textContent, 'Could not run programme: ' + String(message),
                'exception view preserves browser message conversion');
        }
        complete(4, unread, true, {
            get message() {
                throw new Error('timeout must not read message');
            }
        });
        equal(functionRunOutput.textContent, 'Execution request timed out.', 'exception timeout skips message getter');
        equal(functionRun.disabled, true, 'completion never re-enables the button');

        replace('currentMode', () => mode);
        replace('setBusy', () => {});
        replace('setActionRunning', (button, running) => events.push([button, running]));
        labRequests.post = (request, ...args) => {
            requests.push(request);
            return savedPost(request, ...args);
        };
        labRequests.modeChanged(mode);
        functionTitle.textContent = 'Function';
        functionStyle.dataset.fullText = ' \nexact full programme μ\n ';
        const source = functionStyle.dataset.fullText.trim();
        let fetches = 0;
        window.fetch = () => {
            ++fetches;
            throw new Error('ineligible card cannot fetch');
        };
        functionRun.disabled = true;
        await runFunctionCard();
        functionRun.disabled = false;
        functionTitle.textContent = 'Not Function';
        await runFunctionCard();
        functionTitle.textContent = 'Function';
        functionStyle.dataset.fullText = '  ';
        await runFunctionCard();
        equal(fetches, 0, 'disabled, wrong-title and empty-source cases never fetch');
        equal(
            functionRunOutput.textContent, 'No Function programme is available. Evaluate an input first.',
            'empty eligible source reaches availability projection');
        functionStyle.dataset.fullText = source;

        for (const scenario
                 of ['success', 'empty success', 'native failure', 'HTTP failure', 'network failure',
                     'undefined message', 'null message', 'timeout response', 'timeout catch', 'stale success',
                     'stale failure', 'stale catch']) {
            const gate = deferred(), entered = deferred();
            let signal;
            functionRun.disabled = false;
            functionTitle.textContent = 'Function';
            window.fetch = (url, options) => {
                entered.resolve();
                ++fetches;
                equal(String(url), '/function-run', 'native programme endpoint');
                const payload = labWire.decode(options.body);
                equal(payload.source, source, 'executes full source, not abbreviated visible text');
                signal = options.signal;
                return gate.promise;
            };
            const task = runFunctionCard();
            pending.push({task, gate});
            await Promise.race([
                entered.promise, task.then(() => {
                    throw new Error('Function: request was not started');
                })
            ]);
            const request = requests.at(-1);
            check(request, 'real wrapper creates a native request');
            equal(functionRunOutput.textContent, 'Running…', 'real wrapper projects running status');
            equal(functionRun.disabled, true, 'real wrapper disables button during request');
            check(!functionRunResult.classList.contains('hidden'), 'real wrapper reveals output');
            const stale = scenario.startsWith('stale');
            let newer = null;
            if (stale) {
                newer = labRequests.begin('evaluate', mode);
                requests.push(newer);
                check(newer && signal.aborted, 'replacement evaluation cancels programme request');
                functionRunOutput.textContent = 'newer output';
                functionRun.disabled = true;
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
                equal(functionRunOutput.textContent, 'newer output', 'stale completion preserves newer output');
                equal(functionRun.disabled, true, 'stale finaliser cannot enable a newer button');
                check(labRequests.busy(), 'stale finaliser preserves replacement evaluation ownership');
            } else {
                const expected = {
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
                equal(functionRunOutput.textContent, expected[scenario], 'real wrapper completion: ' + scenario);
                equal(functionRun.disabled, false, 'current completion re-enables RUN');
                check(!labRequests.current(request), 'completed request releases native ownership');
            }
            if (newer)
                labRequests.finish(newer);
        }
        // A diagnostic getter may itself fail; the native continuation must still run the original finaliser.
        const diagnosticFailure = new Error('diagnostic getter failure');
        window.fetch = async () => {
            throw {
                get message() {
                    throw diagnosticFailure;
                }
            };
        };
        functionRun.disabled = false;
        let propagated;
        try {
            await runFunctionCard();
        } catch (error) {
            propagated = error;
        }
        equal(propagated, diagnosticFailure, 'diagnostic getter failure propagates unchanged');
        equal(functionRun.disabled, false, 'diagnostic getter failure still enables RUN');
        check(!labRequests.current(requests.at(-1)), 'diagnostic getter failure releases ownership');

        check(events.some(([button, running]) => button === functionRun && running), 'host starts RUN indicator');
        check(events.some(([button, running]) => button === functionRun && !running), 'host clears RUN indicator');
    } finally {
        for (const item of pending) {
            item.gate.resolve(response({ok: false, error: 'fixture cleanup'}));
            try {
                await item.task;
            } catch (_) {
                // Preserve the original assertion while releasing suspended fixtures.
            }
        }
        for (const request of requests)
            if (request)
                labRequests.finish(request);
        window.fetch = savedFetch;
        labRequests.post = savedPost;
        for (const [name, callback] of originals) window[name] = callback;
        labRequests.modeChanged(savedMode);
        for (const {node, children, attributes} of savedNodes) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [name, value] of attributes) node.setAttribute(name, value);
            node.replaceChildren(...children);
        }
    }
};
