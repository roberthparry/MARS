/** Native transport sequencing, lazy diagnostics, publication policy and unconditional request cleanup. */
window.checkLabTransportFlows = async function checkLabTransportFlows() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Transport flow: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const complete = async (kind, frame, registry) => {
        await labDOM.flow(kind, frame, null, registry);
        return frame.result;
    };
    const rejected = async (task, expected, message) => {
        let threw = false, caught;
        try {
            await task;
        } catch (error) {
            threw = true;
            caught = error;
        }
        check(threw, message + ': must reject');
        equal(caught, expected, message + ': exact thrown value');
    };
    const events = [], owner = {};
    let current = true, admitted = owner, finishFailure, ownershipFailure;
    const execution = {
        begin: frame => {
            events.push('begin');
            equal(frame.operation, 'evaluate', 'operation reaches admission');
            return admitted;
        },
        callback: frame => {
            events.push('callback');
            equal(frame.request, owner, 'callback receives admitted request identity');
            return frame.callback(frame.request);
        },
        current: request => {
            events.push('current');
            equal(request, owner, 'ownership check receives admitted identity');
            if (ownershipFailure)
                throw ownershipFailure;
            return current;
        },
        finish: request => {
            events.push('finish');
            equal(request, owner, 'finaliser receives admitted identity');
            if (finishFailure)
                throw finishFailure;
        },
        rethrow: frame => { throw frame.exception; },
        renderError: error => events.push('error:' + error),
        resetDigits: () => events.push('digits'),
        clearResults: options => {
            equal(options.keepBindings, true, 'UI recovery preserves binding controls');
            events.push('clear');
        },
        status: value => events.push('status:' + value)
    };
    for (const kind of [66, 67]) {
        for (const value of [undefined, null, false, 0, NaN, '', Symbol('callback value'), {exact: true}]) {
            events.length = 0;
            equal(await complete(kind, {operation: 'evaluate', callback: () => value}, execution), value,
                  'successful callback value remains uncoerced');
            equal(events.join(','), 'begin,callback,finish', 'successful request finalisation order');
        }
        events.length = 0;
        admitted = null;
        equal(await complete(kind, {operation: 'evaluate', callback: () => 'must not run'}, execution), undefined,
              'rejected admission returns undefined');
        equal(events.join(','), 'begin', 'rejected admission does not callback or finalise');
        admitted = owner;
    }

    const beginFailure = new Error('admission failed'), originalBegin = execution.begin;
    execution.begin = () => { events.push('begin-failed'); throw beginFailure; };
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => 1}, execution), beginFailure,
                       'failed admission preserves exception');
        equal(events.join(','), 'begin-failed', 'failed admission has no allocated request to finalise');
    }
    execution.begin = originalBegin;

    for (const failure of [undefined, null, false, 0, '', Symbol('thrown value'), {exact: true}]) {
        events.length = 0;
        await rejected(complete(66, {operation: 'evaluate', callback: () => { throw failure; }}, execution), failure,
                       'ordinary callback rejection');
        equal(events.join(','), 'begin,callback,current,finish', 'ordinary error checks ownership before cleanup');
    }
    let errorReads = 0;
    const unreadError = {
        get name() { throw new Error('Abort name must not be inspected'); },
        get message() { throw new Error('Message must not be inspected'); },
        toString() {
            ++errorReads;
            throw new Error('Stale error must not be formatted');
        }
    };
    current = false;
    for (const kind of [66, 67]) {
        events.length = 0;
        equal(await complete(kind, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution), undefined,
              'stale rejection is suppressed');
        equal(events.join(','), 'begin,callback,current,finish', 'stale rejection still finalises');
        equal(errorReads, 0, 'stale exception getters are lazy');
    }
    current = true;
    events.length = 0;
    const displayError = {
        get name() { throw new Error('UI path must not inspect abort name'); },
        get message() { throw new Error('UI path must not inspect message separately'); },
        toString() { ++errorReads; return 'display error'; }
    };
    equal(await complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution), undefined,
          'handled UI rejection returns undefined');
    equal(errorReads, 1, 'current UI error is stringified exactly once');
    equal(events.join(','), 'begin,callback,current,error:display error,digits,clear,status:Error,finish',
          'UI recovery order precedes finalisation');

    const recoveryFailure = new Error('recovery failed'), originalRender = execution.renderError;
    execution.renderError = () => { events.push('render-failed'); throw recoveryFailure; };
    events.length = 0;
    await rejected(complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution),
                   recoveryFailure, 'recovery failure reaches outer request catch');
    equal(events.join(','), 'begin,callback,current,render-failed,current,finish',
          'recovery failure checks ownership again and still finalises');
    execution.renderError = () => { events.push('render-stale'); current = false; throw recoveryFailure; };
    events.length = 0;
    equal(await complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution), undefined,
          'superseded recovery failure is suppressed by ordinary outer guard');
    equal(events.join(','), 'begin,callback,current,render-stale,current,finish',
          'superseded recovery still finalises');
    current = true;
    execution.renderError = originalRender;

    const conversionFailure = new Error('conversion failed');
    events.length = 0;
    await rejected(complete(67, {operation: 'evaluate', callback: () => {
        throw {toString() { throw conversionFailure; }};
    }}, execution), conversionFailure, 'error conversion failure is preserved');
    equal(events.join(','), 'begin,callback,current,current,finish', 'conversion failure still finalises');

    ownershipFailure = new Error('ownership failed');
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution),
                       ownershipFailure, 'ownership-check failure');
        equal(events.filter(value => value === 'finish').length, 1, 'ownership failure finalises exactly once');
    }
    ownershipFailure = null;
    finishFailure = new Error('finish failed');
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => 42}, execution), finishFailure,
                       'finaliser failure overrides successful callback');
        equal(events.join(','), 'begin,callback,finish', 'failed finaliser is not retried');
    }
    events.length = 0;
    await rejected(complete(66, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution),
                   finishFailure, 'finaliser failure overrides callback rejection');
    finishFailure = null;

    let response, data, assertion = 0, failAssertion = 0;
    const obsolete = new DOMException('Obsolete Lab request', 'AbortError');
    const posting = {
        assertCurrent: () => {
            events.push('assert');
            if (++assertion === failAssertion)
                throw obsolete;
        },
        endpoint: request => {
            equal(request, owner, 'POST endpoint resolves admitted request');
            events.push('endpoint');
            return '/native-endpoint';
        },
        fetch: async (endpoint, frame) => {
            equal(endpoint, frame.request ? '/native-endpoint' : '/fallback', 'POST endpoint selection');
            events.push('fetch');
            return response;
        },
        decode: async value => {
            equal(value, response, 'decode receives exact response');
            events.push('decode');
            return data;
        },
        presentation: frame => { equal(frame.data, data, 'presentation retains payload identity'); events.push('presentation'); },
        syntax: frame => { equal(frame.data, data, 'syntax retains payload identity'); events.push('syntax'); },
        result: frame => ({response: frame.response, data: frame.data}),
        invalidEndpoint: () => { throw new Error('Invalid native request endpoint'); }
    };
    for (const ok of [undefined, null, false, true, 0, '', 'no']) {
        for (const httpOk of [false, true]) {
            events.length = 0;
            assertion = 0;
            response = {ok: httpOk};
            data = {ok};
            const result = await complete(65, {request: owner, payload: {}}, posting);
            equal(result.response, response, 'POST preserves Response identity');
            equal(result.data, data, 'POST preserves decoded object identity');
            equal(events.join(','), 'assert,endpoint,fetch,assert,decode,assert' +
                  (httpOk && ok !== false ? ',presentation,syntax' : ''), 'strict false metadata publication policy');
        }
    }
    for (const missing of [undefined, null]) {
        data = missing;
        response = {ok: true};
        events.length = 0;
        assertion = 0;
        const result = await complete(65, {request: null, payload: {}, fallback: '/fallback'}, posting);
        equal(result.data, missing, 'null and undefined decoded values remain distinct');
        equal(events.join(','), 'assert,fetch,assert,decode,assert,presentation,syntax', 'fallback POST order');
    }
    response = {ok: false};
    data = {get ok() { throw new Error('HTTP failure must not inspect data.ok'); }};
    await complete(65, {request: owner}, posting);
    response = {ok: true};
    data = {ok: true};
    for (failAssertion = 1; failAssertion <= 3; ++failAssertion) {
        events.length = 0;
        assertion = 0;
        await rejected(complete(65, {request: owner}, posting), obsolete, 'POST stale guard');
        check(!events.includes('presentation') && !events.includes('syntax'), 'stale POST never publishes metadata');
        equal(events.includes('decode'), failAssertion === 3, 'staleness is checked before and after decoding');
    }
    failAssertion = 0;
    const native = labWire.exports();
    equal(native.lab_transport_policy(0, 2, 2), 3, 'Evaluate on main selects button and invalidation');
    equal(native.lab_transport_policy(1, 2, 2), 2, 'other main request selects invalidation only');
    equal(native.lab_transport_policy(0, 3, 2), 1, 'Evaluate button policy is independent of channel');
    equal(native.lab_transport_policy(999, 3, 2), 0, 'background cancellation selects no main UI effects');
};
