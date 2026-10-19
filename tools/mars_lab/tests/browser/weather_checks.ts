/**
 * @file weather_checks.ts
 * @brief Weather completion policy and request-ownership regressions.
 *
 * Uses native completion plans and real request wrappers with controlled services to check stale results and asynchronous ownership.
 */

/** @brief Suspended weather request released during fixture cleanup. */
interface LabWeatherPendingRequest {
    task: Promise<unknown>;
    gate: LabTestResponseGate;
}

/** @brief Wire fields captured by the weather fetch adapter. */
interface LabWeatherRequestPayload {
    date: string;
    latitude: string;
    longitude: string;
}

/** @brief Check native weather completion policy and real request-wrapper ownership. */
window.checkLabWeather = async function checkLabWeather() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Weather: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    /** @brief Build a completion plan without applying its deferred services. */
    function policy(outcome: number, overview: unknown, data: unknown): LabWorksheetTestPlan {
        return labDOM.call<LabWorksheetTestPlan>('lab_evaluation_weather_response', outcome, overview, data);
    }
    let reads = 0;
    const unread = new Proxy({}, {
        get(_target, key) {
            if (key === 'mode')
                return undefined;
            ++reads;
            throw new Error('Weather policy must not read payload fields');
        }
    });
    const before = WorksheetContext.rendered.innerHTML;
    for (const outcome of [0, 3, -1, 5, 2147483647])
        equal(policy(outcome, unread, unread), null, 'stale and invalid outcomes have no completion plan');
    for (const outcome of [1, 4]) {
        const plan = policy(outcome, unread, unread);
        equal(plan.calls.length, 1, 'failed weather has exactly one effect');
        equal(plan.calls[0].service, 'setStatus', 'failure never installs weather');
        equal(plan.calls[0].args.length, 1, 'failure status arity');
        equal(plan.calls[0].args[0], 'Weather unavailable', 'failure and exception share a status');
    }
    const success = policy(2, unread, unread);
    equal(
        success.calls.map(call => call.service).join(','), 'installWeather,setStatus',
        'successful installation precedes Ready');
    equal(success.calls[0].args.length, 2, 'installation receives both responses');
    equal(success.calls[0].args[0], unread, 'overview identity preserved');
    equal(success.calls[0].args[1], unread, 'weather identity preserved');
    equal(success.calls[1].args[0], 'Ready', 'successful completion status');
    equal(reads, 0, 'policy does not inspect or eagerly merge response fields');
    const sky = policy(2, {mode: 'almanac'}, unread);
    equal(sky.calls.map(call => call.service).join(','),
        'almanacRender,almanacAccept,refreshAlmanacLandTotality,setStatus',
        'almanac weather replaces the native worksheet without another foreground evaluation');
    equal(WorksheetContext.rendered.innerHTML, before, 'planning cannot change the result DOM');
    policy(1, null, null);
    equal(success.calls[0].args[0], unread, 'plan references survive later scoped calls');

    const ordered: string[] = [];
    labDOM.services(success, {
        installWeather(overview, data) {
            equal(overview, unread, 'deferred overview identity');
            equal(data, unread, 'deferred response identity');
            equal(policy(0, null, null), null, 'services execute outside the native scope');
            ordered.push('install');
        },
        setStatus: status => ordered.push(status)
    });
    equal(ordered.join(','), 'install,Ready', 'service execution order');

    const savedMode = WorksheetWorkspace.mode.currentMode();
    const savedAlmanac = WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData;
    const savedFetch = window.fetch;
    const originals = new Map<string, unknown>();
    const events: LabWorksheetWeatherEvent[] = [];
    const requests: (WorksheetRequests.MarsApiRequest | null)[] = [];
    const pending: LabWeatherPendingRequest[] = [];

    /** @brief Replace a service and retain its original implementation for cleanup. */
    function replace(name: string, callback: unknown): void {
        if (!originals.has(name))
            originals.set(name, Reflect.get(labTestModuleOwner(name), name));
        Reflect.set(labTestModuleOwner(name), name, callback);
    }

    /** @brief Collect recorded status changes. */
    function named(name: 'status'): ['status', string][];

    /** @brief Collect recorded render calls. */
    function named(name: 'render'): ['render', HTMLElement, null, LabWorksheetWeatherSection[], string][];

    /** @brief Keep only events of the requested kind, retaining their order. */
    function named(name: 'status' | 'render'): LabWorksheetWeatherEvent[] {
        return events.filter(event => event[0] === name);
    }
    /** @brief Encode a reply using the production wire format. */
    function response(data: unknown, status = 200): Response {
        return new Response(labWire.encode(data), {status, headers: {'Content-Type': 'application/x-protobuf'}});
    }

    /** @brief Reject completion before the request enters the fetch adapter. */
    let pendingScenario = '';
    function requestDidNotStart(): never {
        throw new Error('Weather: wrapper completed without making the weather request: ' + pendingScenario);
    }
    const state = Object.freeze({date: '2026-10-10', latitude: '52.7077', longitude: '-2.7541'});
    const calendar = Object.freeze({title: 'Calendar', html: '<p>Native calendar</p>'});
    const overview = Object.freeze({
        overview: 'original overview',
        overview_sections: Object.freeze([calendar, Object.freeze({title: 'Weather', html: '<p>Old weather</p>'})])
    });
    let mode = 'datetime';
    let markupError = false;
    try {
        replace('currentMode', () => mode);
        replace('setStatus', (status: string) => events.push(['status', status]));
        replace('setBusy', () => {});
        replace('setActionRunning', () => {});
        replace('renderDatetimeSections', function recordRender(
            ...args: [HTMLElement, null, LabWorksheetWeatherSection[], string]
        ): void {
            events.push(['render', ...args]);
            if (markupError)
                throw new Error('Native weather markup unavailable');
        });
        WorksheetRequests.labRequests.modeChanged(mode);
        for (const scenario
                 of ['success', 'rejected', 'HTTP failure', 'network failure', 'markup exception', 'stale success',
                     'stale rejected', 'stale network failure', 'mode changed']) {
            pendingScenario = 'datetime ' + scenario;
            mode = 'datetime';
            WorksheetRequests.labRequests.modeChanged(mode);
            const parent = WorksheetRequests.labRequests.begin('evaluate', mode);
            requests.push(parent);
            check(parent, 'fixture creates a parent DateTime evaluation');
            // Background weather remains valid after the parent evaluation has completed.
            WorksheetRequests.labRequests.finish(parent);
            const gate = LabTestBrowser.responseGate();
            const entered = LabTestBrowser.signal();
            let signal!: AbortSignal;
            window.fetch = function captureWeatherFetch(url, options) {
                entered.resolve();
                equal(String(url), '/datetime-weather', 'wrapper uses the weather route');
                const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as LabWeatherRequestPayload;
                equal(payload.date, state.date, 'captured weather date');
                equal(payload.latitude, state.latitude, 'captured weather latitude');
                equal(payload.longitude, state.longitude, 'captured weather longitude');
                signal = options!.signal!;
                return gate.promise;
            };
            markupError = scenario === 'markup exception';
            events.length = 0;
            const task = WorksheetRequests.calendarRequests.refreshDatetimeWeather(parent!.token, state, overview)!;
            pending.push({task, gate});
            await Promise.race([entered.promise, task.then(requestDidNotStart)]);
            equal(
                named('status').map(event => event[1]).join(','), 'Loading weather...',
                'loading status precedes the asynchronous response');
            equal(named('render').length, 0, 'nothing rendered before the response');
            let newer = null;
            const stale = scenario.startsWith('stale') || scenario === 'mode changed';
            if (scenario.startsWith('stale')) {
                newer = WorksheetRequests.labRequests.begin('evaluate', mode);
                requests.push(newer);
                check(newer, 'replacement evaluation is live');
            } else if (scenario === 'mode changed') {
                mode = 'expression';
                WorksheetRequests.labRequests.modeChanged(mode);
            }
            if (stale)
                check(signal.aborted, 'stale weather transport is cancelled');
            events.length = 0;
            if (scenario.endsWith('network failure')) {
                gate.reject(new Error('weather network fixture'));
            } else {
                const ok = !scenario.endsWith('rejected');
                gate.resolve(response(
                    {
                        ok,
                        error: ok ? '' : 'native weather rejection',
                        overview: 'new weather',
                        overview_sections: [{title: 'Weather', html: '<p>New native weather μ</p>'}]
                    },
                    scenario === 'HTTP failure' ? 503 : 200));
            }
            await task;
            if (stale) {
                equal(events.length, 0, 'stale completion neither renders nor changes status');
                if (newer)
                    check(WorksheetRequests.labRequests.busy(), 'stale weather cannot release replacement evaluation ownership');
            } else if (scenario === 'success' || scenario === 'markup exception') {
                equal(named('render').length, 1, 'accepted weather reaches the renderer once');
                const args = named('render')[0];
                equal(args[1], WorksheetContext.rendered, 'weather updates the overview card');
                equal(args[2], null, 'overview has no text-expansion button');
                equal(args[3].length, 2, 'native merge replaces the old weather section');
                equal(args[3][0], calendar, 'non-weather section retains identity');
                equal(args[3][1].html, '<p>New native weather μ</p>', 'native markup remains opaque');
                equal(args[4], 'original overview\nnew weather', 'overview copy text is retained');
                equal(events.map(event => event[0]).join(','), 'render,status', 'status follows rendering');
                equal(
                    named('status')[0][1], markupError ? 'Weather unavailable' : 'Ready',
                    'markup failure cannot publish Ready');
            } else {
                equal(named('render').length, 0, 'HTTP, native and network failures never install weather');
                equal(
                    named('status').map(event => event[1]).join(','), 'Weather unavailable',
                    'current failure publishes unavailable exactly once');
            }
            if (newer)
                WorksheetRequests.labRequests.finish(newer);
            check(!WorksheetRequests.labRequests.busy(), 'scenario releases its foreground work');
        }
        let skyRenders = 0;
        replace('renderAlmanacWorksheet', function recordSkyRender(): void { ++skyRenders; });
        replace('refreshAlmanacLandTotality', function skipLandRefresh(): void {});
        for (const stale of [false, true]) {
            pendingScenario = 'almanac ' + (stale ? 'stale' : 'success');
            mode = 'almanac';
            WorksheetRequests.labRequests.modeChanged(mode);
            const parent = WorksheetRequests.labRequests.begin('evaluate', mode);
            requests.push(parent);
            check(parent, 'fixture creates an Almanac parent');
            WorksheetRequests.labRequests.finish(parent);
            const gate = LabTestBrowser.responseGate();
            const entered = LabTestBrowser.signal();
            const skyState = {...state, mode: 'almanac', time: '12:30:00', elevation: '2', town: '', zone: '0'};
            const admission = labDOM.call<{mode: string}>('lab_flow_begin', 2, {parent: parent!.token, state: skyState});
            equal(admission.mode, 'almanac', 'weather admission follows the captured Almanac mode');
            window.fetch = function captureSkyWeather(url, options) {
                equal(String(url), '/almanac-weather', 'almanac has a separate background endpoint');
                const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as WorksheetCalendarState;
                equal(payload.time, skyState.time, 'weather retains observation time');
                equal(payload.elevation, skyState.elevation, 'weather retains observer height');
                equal(payload.town, '', 'weather retains sea-mode selection');
                entered.resolve();
                return gate.promise;
            };
            const task = WorksheetRequests.calendarRequests.refreshDatetimeWeather(parent!.token, skyState, {mode})!;
            pending.push({task, gate});
            await Promise.race([entered.promise, task.then(function skyDidNotStart(): never {
                throw new Error('Weather: Almanac did not fetch: ' + JSON.stringify(events));
            })]);
            check(!WorksheetRequests.labRequests.busy(), 'pending almanac weather does not hold foreground busy');
            const beforeRefresh = skyRenders;
            if (stale)
                requests.push(WorksheetRequests.labRequests.begin('evaluate', mode));
            gate.resolve(response({ok: true, mode: 'almanac', weather_refresh: true}));
            await task;
            equal(skyRenders, beforeRefresh + (stale ? 0 : 1), 'only current almanac weather may replace the table');
        }
    } finally {
        // Resolve pending fixtures even when an assertion interrupts normal completion.
        for (const item of pending) {
            item.gate.resolve(response({ok: false, error: 'fixture cleanup'}));
            try {
                await item.task;
            } catch (_) {
                // Restore host functions without masking the original assertion.
            }
        }
        for (const request of requests) {
            if (request) {
                WorksheetRequests.labRequests.finish(request);
            }
        }
        window.fetch = savedFetch;
        WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData = savedAlmanac;
        for (const [name, callback] of originals) {
            Reflect.set(labTestModuleOwner(name), name, callback);
        }
        WorksheetRequests.labRequests.modeChanged(savedMode);
    }
};
