/**
 * @file request_checks.ts
 * @brief Native request-state and deferred-fetch regressions.
 *
 * Checks browser request ownership and cancellation with controlled fetch completion under the sequential browser harness.
 */

/// <reference path="./request_checks.d.ts" />
/** Native request state and deferred-fetch regressions, invoked by the sequential browser harness. */
window.checkLabRequests = async function checkLabRequests() {
    const native = labWire.exports();
    const savedMode = WorksheetWorkspace.mode.currentMode();
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error(`C requests: ${message}`);
    };
    const clear = () => {
        for (let channel = 0; channel < 10; ++channel) native.lab_request_cancel(channel);
    };
    const begin = (mode: number, operation: number, scalar = 0, parent = 0) =>
        native.lab_request_begin(mode, operation, 1, 1, scalar, parent) >>> 0;
    try {
        const decoder = new TextDecoder('utf-8', {fatal: true});
        const label = (kind: number, index: number) => decoder.decode(new Uint8Array(
            native.memory.buffer, native.lab_request_text(kind, index), native.lab_request_text_length(kind, index)));
        const names = [
            'evaluate', 'derivative', 'integral', 'function', 'weather', 'goal', 'bindings', 'land', 'datetimeLocation',
            'almanacLocation', 'bindingCommit', 'holidays', 'solverRender', 'solverRestoreRender', 'plot'
        ];
        const paths = [
            '', '/eval', '/matrix-eval', '/equation-eval', '/diffequation-eval', '/integrator-eval', '/datetime-eval',
            '/almanac-eval', '/function-run', '/datetime-weather', '/goal_seek', '/almanac-land-totality',
            '/datetime-jurisdiction-location', '/datetime-jurisdiction-location', '/render_TeX'
        ];
        check(native.lab_request_operation_count() === names.length, 'operation catalogue count');
        [names, paths, ['', 'integral', 'bindings', 'plot']].forEach((entries, kind) => {
            entries.forEach((expected, index) => check(label(kind, index) === expected, 'native catalogue label'));
            check(label(kind, entries.length) === '' && label(kind, -1) === '', 'catalogue index bounds');
        });
        check(label(3, 0) === '' && label(-1, 0) === '', 'catalogue kind bounds');
        check(!WorksheetRequests.labRequests.begin('unknown') && !WorksheetRequests.labRequests.begin('toString'), 'unknown host operation fails closed');

        // Cover every permitted mode/operation/scalar route, including all fixed background endpoints.
        const masks = [127, 9, 9, 127, 32, 1, 1, 64, 32, 64, 31, 32, 4, 4, 23];
        const fixed = [0, 0, 0, 8, 9, 10, 0, 11, 12, 13, 0, 6, 14, 14, 1];
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_request_set_mode(mode);
            for (let operation = 0; operation < names.length; ++operation) {
                for (const scalar of [0, 1]) {
                    clear();
                    const parent = [4, 7, 12].includes(operation) ? begin(mode, 0) : 0;
                    const token = begin(mode, operation, scalar, parent);
                    check(!!token === !!(masks[operation] & (1 << mode)), 'operation mode permission');
                    if (!token)
                        continue;
                    const channel = native.lab_request_channel(operation);
                    const expected = operation === 0 ? [1, 3, 4, 2, 5, 6, 7][mode] :
                                                       fixed[operation] || (mode === 3 && !scalar ? 2 : 1);
                    check(native.lab_request_endpoint(channel, token) === expected, 'table-driven endpoint');
                    check(label(1, expected) === paths[expected], 'endpoint wire path');
                    const action = operation === 14 ? 'plot' : operation === 2 ? 'integral' :
                        [6, 10].includes(operation) ? 'bindings' : '';
                    check(label(2, native.lab_request_action(channel, token)) === action, 'table-driven action');
                    native.lab_request_finish(channel, token);
                    check(label(1, native.lab_request_endpoint(channel, token)) === '', 'finished request has no path');
                }
            }
        }
        native.lab_request_set_mode(0);
        const evaluation = begin(0, 0);
        const plot = begin(0, 14, 0, evaluation);
        const plotChannel = native.lab_request_channel(14);
        check(plotChannel !== 0 && plot, 'plot owns a separate channel');
        native.lab_request_finish(0, evaluation);
        check(!native.lab_request_busy() && native.lab_request_live(plotChannel, plot),
              'evaluation completes while its asynchronous plot remains active');
        begin(0, 0);
        check(!native.lab_request_live(plotChannel, plot), 'new evaluation invalidates its old plot');
        check(!begin(0, 14, 0, evaluation), 'obsolete evaluation cannot start a plot');
        clear();
        const endpoints = [1, 3, 4, 2, 5, 6, 7];
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_request_set_mode(mode);
            const token = begin(mode, 0);
            check(
                token && native.lab_request_endpoint(0, token) === endpoints[mode],
                `evaluation endpoint for mode ${mode}`);
            check(native.lab_request_busy() === 1, 'evaluation begins busy state');
            check(native.lab_request_input(0, token, 0, 1) === 0, 'empty evaluation input rejected');
            check(native.lab_request_accept(0, token, 0, 1, 1, 0) === 1, 'HTTP failures rejected');
            check(native.lab_request_accept(0, token, 1, 0, 1, 0) === 1, 'native failures rejected');
            check(native.lab_request_accept(0, token, 1, 1, 1, 1) === (mode === 0 ? 3 : 2), 'partial result policy');
            check(
                native.lab_request_finish(0, token) === 1 && !native.lab_request_busy(),
                'completion releases busy state');
        }
        for (const mode of [0, 3]) {
            native.lab_request_set_mode(mode);
            for (const operation of [1, 2]) {
                for (const scalar of [0, 1]) {
                    const token = begin(mode, operation, scalar);
                    const endpoint = mode === 3 && !scalar ? 2 : 1;
                    check(
                        token && native.lab_request_endpoint(0, token) === endpoint,
                        'full/scalar matrix calculus routing');
                    check(native.lab_request_action(0, token) === (operation === 2 ? 1 : 0), 'calculus action policy');
                    check(
                        native.lab_request_flags(0, token) === (mode === 0 ? 1 : 2),
                        'persistence/transient calculus policy');
                    check(
                        native.lab_request_accept(0, token, 1, 1, 0, 0) === (endpoint === 1 ? 1 : 2),
                        'scalar calculus requires the requested result');
                    check(native.lab_request_input(0, token, 1, 0) === 0, 'calculus requires a variable');
                }
            }
        }
        for (const mode of [1, 2, 4, 5, 6]) {
            native.lab_request_set_mode(mode);
            check(begin(mode, 1) === 0 && begin(mode, 2) === 0, 'calculus disallowed in other modes');
        }
        native.lab_request_set_mode(0);
        const old = begin(0, 0);
        const latest = begin(0, 0);
        check(old !== latest && !native.lab_request_live(0, old), 'latest request supersedes earlier work');
        check(!native.lab_request_finish(0, old) && native.lab_request_busy(), 'old finaliser cannot clear busy state');
        check(!native.lab_request_accept(0, old, 1, 1, 1, 0), 'late successful response is stale');
        const context = native.lab_request_context();
        native.lab_request_set_mode(3);
        native.lab_request_set_mode(0);
        check(native.lab_request_context() !== context && !native.lab_request_live(0, latest), 'away/back stays stale');
        check(
            !begin(99, 0) && !begin(0, 99) && !native.lab_request_begin(0, 1, 1, 0, 0, 0),
            'invalid inputs fail closed');

        const programme = begin(0, 3);
        check(programme && !native.lab_request_busy(), 'Function RUN is independent of editor busy state');
        check(native.lab_request_deadline(1, programme) === 45000, 'Function RUN native deadline');
        check(native.lab_request_timeout(1, programme) && native.lab_request_timed_out(1, programme), 'owned timeout');
        check(native.lab_request_accept(1, programme, 1, 1, 1, 0) === 1, 'late success after deadline is rejected');
        native.lab_request_cancel(1);
        check(!native.lab_request_timed_out(1, programme), 'explicit cancellation loses timeout ownership');

        native.lab_request_set_mode(5);
        check(!begin(5, 4), 'weather requires a parent');
        const date = begin(5, 0);
        check(native.lab_request_finish(0, date), 'datetime main completes');
        const weather = begin(5, 4, 0, date);
        const holidays = begin(5, 11);
        const location = begin(5, 8);
        check(
            weather && holidays && location && !native.lab_request_busy(), 'independent datetime background channels');
        check(
            native.lab_request_endpoint(7, holidays) === 6 && native.lab_request_live(2, weather), 'holiday endpoint');
        begin(5, 0);
        check(
            !native.lab_request_live(2, weather) && !native.lab_request_live(7, holidays) &&
                !native.lab_request_live(5, location),
            'new evaluation invalidates old background work');

        native.lab_request_set_mode(6);
        const cachedLand = begin(6, 7);
        check(cachedLand && native.lab_request_deadline(4, cachedLand) === 22000, 'cached worksheet land request');
        const almanac = begin(6, 0);
        check(!native.lab_request_live(4, cachedLand), 'new almanac invalidates cached land request');
        const land = begin(6, 7, 0, almanac);
        check(land && !begin(6, 7, 0, almanac + 1), 'land rejects an obsolete explicit parent');
        check(begin(6, 9) && native.lab_request_live(4, land), 'almanac location has a separate channel');

        native.lab_request_set_mode(2);
        check(!begin(2, 12), 'solver rendering requires a main result parent');
        const solverMain = begin(2, 0);
        native.lab_request_finish(0, solverMain);
        const solver = begin(2, 12, 0, solverMain);
        check(
            solver && native.lab_request_channel(12) === 8 && native.lab_request_endpoint(8, solver) === 14 &&
                !native.lab_request_busy(),
            'solver rendering is a separate nonbusy render TeX channel');
        check(!begin(2, 12, 0, solverMain + 1), 'solver rejects mismatched result parent');
        begin(2, 0);
        check(!native.lab_request_live(8, solver), 'new main result invalidates solver rendering');
        check(!begin(2, 12, 0, solverMain), 'old card cannot attach solver rendering to a newer main request');
        const nextSolver = begin(2, 12, 0, native.lab_request_latest_main());
        native.lab_request_cancel(0);
        check(!native.lab_request_live(8, nextSolver), 'main cancellation invalidates solver rendering');
        const restoredSolver = begin(2, 13);
        check(
            restoredSolver && native.lab_request_channel(13) === 8 &&
                native.lab_request_endpoint(8, restoredSolver) === 14 &&
                native.lab_request_deadline(8, restoredSolver) === 45000 && !native.lab_request_busy(),
            'restored solver renders independently on the cancellable solver channel');
        check(!begin(2, 13, 0, solverMain), 'restored solver operation never accepts an evaluation parent');
        check(
            native.lab_request_timeout(8, restoredSolver) &&
                native.lab_request_accept(8, restoredSolver, 1, 1, 1, 0) === 1,
            'restored solver rejects successful replies after its deadline');
        begin(2, 0);
        check(!native.lab_request_live(8, restoredSolver), 'new main invalidates restored solver transport');
        native.lab_request_set_mode(0);
        check(!begin(0, 12), 'solver rendering rejects expression mode');
        const binding = begin(0, 6);
        check(
            binding && !native.lab_request_busy() && native.lab_request_action(3, binding) === 2,
            'binding refresh policy');
        const commit = begin(0, 10);
        check(
            commit && native.lab_request_busy() && !native.lab_request_finish(3, binding),
            'binding commit owns busy state');
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_request_set_mode(mode);
            const editing = begin(mode, 10);
            check(Boolean(editing) === (mode < 5), 'binding commits are available only in mathematical modes');
            if (editing)
                native.lab_request_finish(3, editing);
        }
    } finally {
        clear();
        WorksheetRequests.labRequests.modeChanged(savedMode);
    }

    const originalFetch = window.fetch;
    const cacheKey = '__mars_request_cache_fixture__';
    const hadCacheEntry = WorksheetResults.labEvaluationView.caches.compact.has(cacheKey);
    const savedCacheEntry = WorksheetResults.labEvaluationView.caches.compact.get(cacheKey);
    const metadata = (display: string) => ({compact_texts: [{text: cacheKey, display}]});
    const pending: {resolve(value: Response): void; reject(reason: unknown): void; signal: AbortSignal}[] = [];
    const applied: unknown[] = [];
    const response = (body: unknown) => new Response(labWire.encode(body), {headers: {'Content-Type': 'application/x-protobuf'}});
    const collect = () => WorksheetRequests.labRequests.run('evaluate', 'expression', async request => {
        const result = await WorksheetRequests.evaluationTransport.fetchEvaluation('2+3', '', '', '', '', request);
        if (WorksheetRequests.labRequests.outcome(request, result.response, result.data) === 2)
            applied.push(result.data.marker);
    });
    try {
        WorksheetWorkspace.mode.setMode('expression');
        WorksheetRequests.labRequests.modeChanged('expression');
        window.fetch = (url, options) => {
            if (String(url) !== '/eval')
                return originalFetch(url, options);
            if (!options?.signal) throw new Error('Missing request signal');
            const signal = options.signal;
            return new Promise<Response>((resolve, reject) => pending.push({resolve, reject, signal}));
        };
        const old = collect();
        const latest = collect();
        check(pending.length === 2 && pending[0].signal.aborted, 'supersession aborts host resources');
        pending[0].resolve(response({ok: true, marker: 'obsolete'}));
        await old;
        check(WorksheetRequests.labRequests.busy() && WorksheetContext.run.disabled && !applied.length, 'stale completion retains newer busy state');
        pending[1].resolve(response({ok: true, marker: 'latest'}));
        await latest;
        check(!WorksheetRequests.labRequests.busy() && applied.join() === 'latest', 'only newest payload is applied');

        const modeRequest = collect();
        WorksheetWorkspace.mode.setMode('matrix');
        WorksheetRequests.labRequests.modeChanged('matrix');
        WorksheetWorkspace.mode.setMode('expression');
        WorksheetRequests.labRequests.modeChanged('expression');
        pending[2].resolve(response({ok: true, marker: 'old mode'}));
        await modeRequest;
        check(applied.join() === 'latest' && !WorksheetRequests.labRequests.busy(), 'mode round trip rejects late fetch');

        const failed = collect();
        pending[3].reject(new Error('fixture network failure'));
        let rejected = false;
        try {
            await failed;
        } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
            rejected = error.message === 'fixture network failure';
        }
        check(rejected && !WorksheetRequests.labRequests.busy(), 'owned network failure releases native busy state');

        WorksheetResults.labEvaluationView.caches.compact.set(cacheKey, 'retained');
        let streamController!: ReadableStreamDefaultController<Uint8Array<ArrayBuffer>>, decodingStarted!: () => void;
        const decoding = new Promise<void>(resolve => {
            decodingStarted = resolve;
        });
        const stream = new ReadableStream<Uint8Array<ArrayBuffer>>(
            {
                start(controller) {
                    streamController = controller;
                },
                pull() {
                    decodingStarted();
                }
            },
            {highWaterMark: 0});
        window.fetch = async () => new Response(stream, {headers: {'Content-Type': 'application/x-protobuf'}});
        const staleCacheRequest = collect();
        await decoding;
        const cacheOwner = WorksheetRequests.labRequests.begin('evaluate', 'expression');
        streamController.enqueue(labWire.encode({ok: true, marker: 'stale cache', presentation: metadata('obsolete')}));
        streamController.close();
        await staleCacheRequest;
        check(
            WorksheetResults.labEvaluationView.caches.compact.get(cacheKey) === 'retained',
            'supersession during body decoding cannot install stale presentation metadata');
        check(WorksheetRequests.labRequests.busy(), 'stale cache request finaliser preserves the current owner');
        WorksheetRequests.labRequests.finish(cacheOwner);

        for (const [status, ok, expected] of [
                 [200, true, 'accepted'], [200, false, 'accepted'], [500, true, 'accepted']] as const) {
            const cacheRequest = WorksheetRequests.labRequests.begin('evaluate', 'expression');
            window.fetch = async () => new Response(
                labWire.encode({ok, presentation: metadata(status === 200 && ok ? 'accepted' : 'rejected')}),
                {status, headers: {'Content-Type': 'application/x-protobuf'}});
            try {
                await WorksheetRequests.labRequests.post(cacheRequest, {expression: '2+3'}, '');
                check(
                    WorksheetResults.labEvaluationView.caches.compact.get(cacheKey) === expected,
                    'only current successful responses install presentation metadata');
            } finally {
                WorksheetRequests.labRequests.finish(cacheRequest);
            }
        }

        const mode = WorksheetWorkspace.mode.currentMode();
        for (const scalar of [false, true]) {
            WorksheetWorkspace.mode.setMode('matrix');
            WorksheetRequests.labRequests.modeChanged('matrix');
            const request = WorksheetRequests.labRequests.begin('integral', 'matrix', {scalar});
            let sent!: {url: string; body: {action?: string; persist_expression?: boolean; transient?: boolean}};
            window.fetch = async (url, options) => {
                if (!(options?.body instanceof Uint8Array)) throw new Error('Missing binary body');
                sent = {url: String(url), body: labWire.decode(options.body as Uint8Array<ArrayBuffer>) as typeof sent.body};
                return response({ok: true});
            };
            try {
                if (scalar)
                    await WorksheetRequests.evaluationTransport.fetchEvaluation('x', 'x', '', '', '', request);
                else
                    await WorksheetRequests.evaluationTransport.fetchMatrixEvaluation({matrixText: '[x]', operation: 'eval', operand: '', request});
                check(sent.url === (scalar ? '/eval' : '/matrix-eval'), 'matrix/scalar API uses native endpoint');
                check(
                    scalar ? sent.body.action === 'integral' && !sent.body.persist_expression :
                             sent.body.transient === true,
                    'matrix/scalar API uses native persistence policy');
            } finally {
                WorksheetRequests.labRequests.finish(request);
            }
        }
        WorksheetWorkspace.mode.setMode('diffequation');
        WorksheetRequests.labRequests.modeChanged('diffequation');
        const solverMain = WorksheetRequests.labRequests.begin('evaluate', 'diffequation');
        WorksheetRequests.labRequests.finish(solverMain);
        const solverRequest = WorksheetRequests.labRequests.begin('solverRender', 'diffequation', {parent: solverMain!.token});
        let solverPending!: {resolve(value: Response): void; signal: AbortSignal};
        window.fetch = (url, options) => {
            check(String(url) === '/render_TeX', 'solver adapter selects the native render endpoint');
            if (!options?.signal) throw new Error('Missing solver signal');
            const signal = options.signal;
            return new Promise<Response>(resolve => {
                solverPending = {resolve, signal};
            });
        };
        const rendering =
            WorksheetRequests.labRequests.post(solverRequest, {tex: 'x'}, '').then(() => false, error => error.name === 'AbortError');
        const newerSolverMain = WorksheetRequests.labRequests.begin('evaluate', 'diffequation');
        check(solverPending.signal.aborted, 'new evaluation aborts dependent solver rendering');
        solverPending.resolve(response({ok: true, svg: '<svg/>'}));
        check(await rendering, 'late solver render is rejected before reaching layout');
        WorksheetRequests.labRequests.finish(solverRequest);
        check(WorksheetRequests.labRequests.busy(), 'old solver finaliser cannot release main busy state');
        WorksheetRequests.labRequests.finish(newerSolverMain);
        WorksheetWorkspace.mode.setMode(mode);
    } finally {
        window.fetch = originalFetch;
        if (hadCacheEntry)
            WorksheetResults.labEvaluationView.caches.compact.set(cacheKey, savedCacheEntry!);
        else
            WorksheetResults.labEvaluationView.caches.compact.delete(cacheKey);
        clear();
        WorksheetWorkspace.mode.setMode(savedMode);
        WorksheetRequests.labRequests.modeChanged(savedMode);
        WorksheetWorkspace.presentation.syncModeUI();
    }
};
