/** Native weather completion policy and real request-wrapper ownership regressions. */
window.checkLabWeather = async function checkLabWeather() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Weather: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const policy = (outcome, overview, data) => labDOM.call('lab_evaluation_weather_response', outcome, overview, data);
    let reads = 0;
    const unread = new Proxy({}, {
        get() {
            ++reads;
            throw new Error('Weather policy must not read payload fields');
        }
    });
    const before = rendered.innerHTML;
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
    equal(rendered.innerHTML, before, 'planning cannot change the result DOM');
    policy(1, null, null);
    equal(success.calls[0].args[0], unread, 'plan references survive later scoped calls');

    const ordered = [];
    labDOM.services(success, {
        installWeather: (overview, data) => {
            equal(overview, unread, 'deferred overview identity');
            equal(data, unread, 'deferred response identity');
            equal(policy(0, null, null), null, 'services execute outside the native scope');
            ordered.push('install');
        },
        setStatus: status => ordered.push(status)
    });
    equal(ordered.join(','), 'install,Ready', 'service execution order');

    const savedMode = currentMode(), savedFetch = window.fetch;
    const originals = new Map(), events = [], requests = [], pending = [];
    const replace = (name, callback) => {
        if (!originals.has(name))
            originals.set(name, window[name]);
        window[name] = callback;
    };
    const named = name => events.filter(event => event[0] === name);
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
    const state = Object.freeze({date: '2026-10-10', latitude: '52.7077', longitude: '-2.7541'});
    const calendar = Object.freeze({title: 'Calendar', html: '<p>Native calendar</p>'});
    const overview = Object.freeze({
        overview: 'original overview',
        overview_sections: Object.freeze([calendar, Object.freeze({title: 'Weather', html: '<p>Old weather</p>'})])
    });
    let mode = 'datetime', markupError = false;
    try {
        replace('currentMode', () => mode);
        replace('setStatus', status => events.push(['status', status]));
        replace('setBusy', () => {});
        replace('setActionRunning', () => {});
        replace('renderDatetimeSections', (...args) => {
            events.push(['render', ...args]);
            if (markupError)
                throw new Error('Native weather markup unavailable');
        });
        labRequests.modeChanged(mode);
        for (const scenario
                 of ['success', 'rejected', 'HTTP failure', 'network failure', 'markup exception', 'stale success',
                     'stale rejected', 'stale network failure', 'mode changed']) {
            mode = 'datetime';
            labRequests.modeChanged(mode);
            const parent = labRequests.begin('evaluate', mode);
            requests.push(parent);
            check(parent, 'fixture creates a parent DateTime evaluation');
            // Background weather remains valid after the parent evaluation has completed.
            labRequests.finish(parent);
            const gate = deferred(), entered = deferred();
            let signal;
            window.fetch = (url, options) => {
                entered.resolve();
                equal(String(url), '/datetime-weather', 'wrapper uses the weather route');
                const payload = labWire.decode(options.body);
                equal(payload.date, state.date, 'captured weather date');
                equal(payload.latitude, state.latitude, 'captured weather latitude');
                equal(payload.longitude, state.longitude, 'captured weather longitude');
                signal = options.signal;
                return gate.promise;
            };
            markupError = scenario === 'markup exception';
            events.length = 0;
            const task = refreshDatetimeWeather(parent.token, state, overview);
            pending.push({task, gate});
            await Promise.race([
                entered.promise, task.then(() => {
                    throw new Error('Weather: wrapper completed without making the weather request');
                })
            ]);
            equal(
                named('status').map(event => event[1]).join(','), 'Loading weather...',
                'loading status precedes the asynchronous response');
            equal(named('render').length, 0, 'nothing rendered before the response');
            let newer = null;
            const stale = scenario.startsWith('stale') || scenario === 'mode changed';
            if (scenario.startsWith('stale')) {
                newer = labRequests.begin('evaluate', mode);
                requests.push(newer);
                check(newer, 'replacement evaluation is live');
            } else if (scenario === 'mode changed') {
                mode = 'expression';
                labRequests.modeChanged(mode);
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
                    check(labRequests.busy(), 'stale weather cannot release replacement evaluation ownership');
            } else if (scenario === 'success' || scenario === 'markup exception') {
                equal(named('render').length, 1, 'accepted weather reaches the renderer once');
                const args = named('render')[0];
                equal(args[1], rendered, 'weather updates the overview card');
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
                labRequests.finish(newer);
            check(!labRequests.busy(), 'scenario releases its foreground work');
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
        for (const request of requests)
            if (request)
                labRequests.finish(request);
        window.fetch = savedFetch;
        for (const [name, callback] of originals) window[name] = callback;
        labRequests.modeChanged(savedMode);
    }
};
