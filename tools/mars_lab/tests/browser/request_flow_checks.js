/** Native request continuations: exact transport order, freshness and recovery boundaries. */
window.checkLabRequestFlows = async function checkLabRequestFlows() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Request flow: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const events = [];
    const fail = message => {
        throw new Error(message);
    };
    const abort = message => {
        throw new DOMException(message, 'AbortError');
    };
    const rejected = async (promise, name, message) => {
        let error;
        try {
            await promise;
        } catch (caught) {
            error = caught;
        }
        equal(error?.name, name, 'error class');
        equal(error?.message, message, 'error diagnostic');
    };
    const advance = (kind, frame, services, view = {}) => labDOM.flow(kind, frame, view, services);

    // Forms decode failures propagate without being mistaken for native preparation errors.
    for (const [status, ok, diagnostic] of [[200, true, ''], [200, false, 'native forms'], [503, true, '']]) {
        events.length = 0;
        const data = {ok, error: diagnostic, rows: [{name: 'α'}]}, signal = {};
        const response = {ok: status === 200, status};
        const task = advance(12, {payload: {action: 'integrator'}, options: {signal}}, {
            rawPost: (url, payload, sentSignal) => {
                events.push('post');
                equal(url, '/forms', 'forms endpoint');
                equal(payload.action, 'integrator', 'forms payload is opaque');
                equal(sentSignal, signal, 'borrowed abort signal');
                return response;
            },
            responseData: supplied => {
                equal(supplied, response, 'same response decoded');
                events.push('decode');
                return data;
            },
            fail
        });
        if (status === 200 && ok)
            equal(await task, data, 'forms returns the exact native result');
        else
            await rejected(task, 'Error', diagnostic || `Form preparation failed (${status})`);
        equal(events.join(), 'post,decode', 'forms always decode before validating');
    }

    // Presentation metadata must not publish after a mode/context transition during decoding.
    for (const owned of [false, true]) {
        for (const stale of [false, true]) {
            events.length = 0;
            let context = 11, current = true;
            const request = owned ? {controller: {signal: {}}} : null;
            const data = {ok: true, editor: {expression: 'opaque μ'}};
            const task = advance(13, {payload: {action: 'editor'}, request}, {
                requestContext: () => context,
                requestCurrent: supplied => {
                    equal(supplied, request, 'presentation checks the owned request');
                    return current;
                },
                rawPost: (url, _payload, signal) => {
                    equal(url, '/presentation', 'presentation endpoint');
                    equal(signal, request?.controller.signal ?? null, 'presentation signal selection');
                    events.push('post');
                    return {ok: true};
                },
                responseData: () => {
                    events.push('decode');
                    if (stale) {
                        ++context;
                        current = false;
                    }
                    return data;
                },
                installPresentation: supplied => {
                    equal(supplied, data, 'only native metadata is installed');
                    events.push('install');
                },
                abort,
                fail
            });
            if (stale)
                await rejected(task, 'AbortError', 'Obsolete presentation request');
            else
                equal(await task, data, 'presentation returns the same data object');
            equal(events.join(), stale ? 'post,decode' : 'post,decode,install', 'publication freshness gate');
        }
    }

    // Editor cache hits must not start requests or inspect request context.
    const cached = {expression: 'cached'};
    equal(
        await advance(13, {editor: true, text: '  μ  '}, {}, {editors: new Map([['μ', cached]])}), cached,
        'native editor cache normalises only surrounding whitespace');
    for (const sameContext of [false, true]) {
        const pending = {context: sameContext ? 7 : 6, promise: Promise.resolve(cached)};
        const requests = new Map([['μ', pending]]), replacement = {expression: 'replacement'};
        let starts = 0;
        const value = await advance(
            13, {editor: true, text: ' μ '}, {
                requestContext: () => 7,
                editorRequests: () => requests,
                awaitValue: promise => promise,
                editorPromise: (source, next, payload) => {
                    ++starts;
                    equal(source, 'μ', 'deduplicated exact source');
                    equal(next.context, 7, 'replacement owns current context');
                    equal(payload.operation, 'analyse', 'editor preparation operation');
                    equal(payload.text, 'μ', 'editor preparation source');
                    return replacement;
                }
            },
            {editors: new Map()});
        equal(value, sameContext ? cached : replacement, 'pending editor request context selection');
        equal(starts, sameContext ? 0 : 1, 'only a changed context starts replacement preparation');
    }

    // The integrator rejects stale preparations before reading later guards or publishing rows.
    for (const staleAt of ['prepared', 'context', 'request', 'text', 'rows', 'none']) {
        events.length = 0;
        const request = {}, rows = [{name: 'x'}],
              snapshot = {text: 'exp(Li(x))', rows, context: 9, rowsText: 'x = 0 .. 1'};
        const bounds = [{name: 'x', lower: '0', upper: '1'}], payload = {}, result = {};
        const services = {
            integratorSnapshot: () => snapshot,
            refreshIntegratorForms: (supplied, text) => {
                equal(supplied, rows, 'preparation sees the captured rows');
                equal(text, snapshot.text, 'preparation sees the captured source');
                events.push('prepare');
                return staleAt === 'prepared' ? null : rows;
            },
            requestContext: () => {
                events.push('context');
                return staleAt === 'context' ? 10 : 9;
            },
            requestCurrent: () => {
                events.push('current');
                return staleAt !== 'request';
            },
            integratorText: () => {
                events.push('text');
                return staleAt === 'text' ? 'other' : snapshot.text;
            },
            integratorRowsText: () => {
                events.push('rows');
                return staleAt === 'rows' ? 'other' : snapshot.rowsText;
            },
            requestCancel: operation => {
                equal(operation, 'evaluate', 'only evaluation is cancelled');
                events.push('cancel');
            },
            activeIntegratorRows: () => ({rows, bounds}),
            renderIntegratorRows: supplied => {
                equal(supplied, rows, 'accepted rows rendered');
                events.push('render');
            },
            native: (name, supplied) => {
                equal(name, 'lab_payload_bounds_error', 'native bounds validation');
                equal(supplied, bounds, 'selected bounds are validated');
                return '';
            },
            saveWorksheetState: mode => {
                equal(mode, 'integrator', 'accepted worksheet saved');
                events.push('save');
            },
            integratorPayload: () => payload,
            post: (supplied, body, endpoint) => {
                equal(supplied, request, 'integrator request ownership');
                equal(body, payload, 'integrator payload unchanged');
                equal(endpoint, '/integrator-eval', 'integrator endpoint');
                events.push('post');
                return result;
            },
            abort,
            fail
        };
        const task = advance(8, {request}, services);
        if (staleAt === 'none') {
            equal(await task, result, 'integrator returns native result');
            equal(events.join(), 'prepare,context,current,text,rows,render,save,post', 'integrator publication order');
        } else {
            await rejected(task, 'AbortError', 'Obsolete integrator form preparation');
            check(!events.includes('render') && !events.includes('post'), 'stale rows never publish or evaluate');
            if (staleAt === 'prepared')
                check(!events.includes('context'), 'empty prepared rows short-circuit context getter');
            if (['prepared', 'context', 'request'].includes(staleAt))
                check(!events.includes('text'), 'early staleness short-circuits editor getter');
            if (staleAt !== 'request')
                check(events.includes('cancel'), 'owned obsolete preparation cancels evaluation');
            else
                check(!events.includes('cancel'), 'stale request cannot cancel replacement evaluation');
        }
    }

    // Calculus owns sequencing in C for expressions, whole matrices and scalar matrix results.
    for (const variant of ['expression', 'matrix', 'scalar', 'failure', 'stale']) {
        events.length = 0;
        const matrix = variant === 'matrix' || variant === 'scalar';
        const calculus = {expression: 'native result'}, bindings = [{name: 'x'}];
        const data = {
            ok: variant !== 'failure',
            binding_values: bindings,
            presentation: {calculus: {derivative: calculus}}
        };
        let current = true;
        const request = {}, variables = ['retained'];
        const services = {
            calculusInitial: () =>
                ({mode: matrix ? 'matrix' : 'expression', scalar: variant === 'scalar' ? 'scalar source' : ''}),
            runRequestFlow: (kind, frame, stage, operation, mode, options) => {
                equal(operation, 'derivative', 'calculus operation owns request');
                equal(mode, matrix ? 'matrix' : 'expression', 'calculus mode');
                equal(options.scalar, variant === 'scalar', 'native scalar request flag');
                return advance(kind, {...frame, stage, request}, services);
            },
            calculusCapture: () => ({bindings, variables, differentiable: false}),
            commitBindings: () => events.push('commit bindings'),
            requestCurrent: () => current,
            matrixScalar: () => 'scalar source',
            expressionText: () => 'authored source',
            expressionWithVisibleBindings: text => {
                events.push('assemble');
                return text;
            },
            requestInput: (_request, source, wrt) => {
                equal(source, variant === 'scalar' ? 'scalar source' : 'authored source', 'source chosen after commit');
                equal(wrt, 'x', 'exact calculus variable');
                return true;
            },
            showResults: () => events.push('show'),
            calculusTitle: text => equal(text, 'x derivative RESULT', 'native calculus title'),
            setStatus: text => events.push(text),
            requestPresentation: (payload, owner) => {
                equal(owner, request, 'matrix preparation is owned');
                equal(payload.calculus, 'derivative', 'matrix preparation operation');
                events.push('prepare matrix');
                return {editor: {expression: 'native matrix'}};
            },
            fetchMatrixOptions: options => {
                equal(options.matrixText, 'native matrix', 'native matrix source used');
                equal(options.skipSave, true, 'transient calculus matrix does not replace worksheet');
                return {response: {ok: true}, data};
            },
            fetchCalculus: () => {
                if (variant === 'stale')
                    current = false;
                return {response: {ok: true}, data};
            },
            requestOutcome: () => !current ? 0 :
                variant === 'failure'      ? 1 :
                                             2,
            clearResultDetails: options => {
                equal(options.keepBindings, true, 'calculus retains binding editor');
                events.push('clear');
            },
            clearRenderedError: () => events.push('clear error'),
            displayMatrixResult: supplied => equal(supplied, data, 'whole matrix native result'),
            displayCalculusResult: supplied => equal(supplied, calculus, 'native expression result'),
            valueTitle: text => equal(text, 'Summary', 'matrix without value has summary title'),
            setMatrixPrettyResult: (text, pretty) => {
                equal(text, calculus.expression, 'scalar result source');
                equal(pretty, '', 'scalar result requests native pretty form');
                events.push('pretty scalar');
            },
            derivative: text => equal(text, calculus.expression, 'derivative source cache'),
            calculusVariables: supplied => equal(supplied, variables, 'matrix variables preserved'),
            calculusBindingVariables: supplied => equal(supplied, bindings, 'expression bindings determine variables'),
            calculusDifferentiable: enabled => equal(enabled, !matrix, 'differentiability selection'),
            calculusButtons: () => events.push('buttons'),
            commitModeState: () => events.push('commit state'),
            setRenderedError: text => equal(text, 'Error: No derivative for x', 'native calculus failure fallback'),
            resetRenderedDigits: () => events.push('reset digits'),
            fail
        };
        await advance(7, {wrt: 'x', action: 'derivative'}, services);
        check(events.includes('assemble') === !matrix, 'only expression calculus assembles visible bindings');
        if (variant === 'stale') {
            check(!events.includes('clear') && !events.includes('Ready'), 'stale calculus never publishes');
        } else if (variant === 'failure') {
            equal(events.at(-1), 'Error', 'failed calculus reports error');
            check(
                events.includes('reset digits') && !events.includes('Ready'),
                'failed calculus resets precision control');
        } else {
            equal(events.at(-1), 'Ready', 'successful calculus completes');
            check(
                events.includes('commit state') === (variant === 'matrix'),
                'only whole matrix calculus commits mode state');
            check(events.includes('pretty scalar') === (variant === 'scalar'), 'scalar pretty request stays distinct');
        }
    }
};
