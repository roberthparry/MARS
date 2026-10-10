/** Native location continuations, deferred services and stale browser ownership checks. */
window.checkLabLocationFlow = async function checkLabLocationFlow() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Location flow: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const step = (kind, frame) => labDOM.call('lab_flow_step', kind, frame, {});
    const events = [];
    const run = (kind, frame, services) => labDOM.flow(kind, frame, {}, {
        effects: plan => labDOM.services(plan, services),
        native: (entry, ...args) => labDOM.call(entry, ...args),
        ...services
    });
    const savedRequest = requestLabForms;
    let settle = null, pending = null;
    try {
        const localCard = document.createElement('section'), localBody = document.createElement('div');
        const sections = Object.freeze([{name: 'opaque native calendar'}]);
        const localPlan = labDOM.call('lab_flow_location_local', {value: '  calendar text  '}, sections, localBody, localCard);
        equal(localPlan.calls.map(call => call.service).join(','), 'locRenderLocal,locLocalVisible',
              'local-calendar rendering precedes mode-dependent visibility');
        equal(localPlan.calls[0].args[1], sections, 'calendar sections retain native identity');
        equal(localPlan.calls[0].args[2], 'calendar text', 'fallback text trimmed once by native policy');
        equal(localBody.textContent, '', 'planning does not render');
        equal(localCard.className, '', 'planning does not change visibility');
        const localFailure = new Error('calendar render failed');
        let localCaught = null;
        try {
            labDOM.services(localPlan, {
                locRenderLocal: () => { throw localFailure; },
                locLocalVisible: () => { throw new Error('visibility ran after render failure'); }
            });
        } catch (error) {
            localCaught = error;
        }
        equal(localCaught, localFailure, 'synchronous renderer failure stops visibility');
        const emptyLocal = labDOM.call('lab_flow_location_local', {value: 0}, null, null, localCard);
        equal(emptyLocal.calls[0].service, 'locLocalHidden', 'empty fallback hides without reading current mode');
        const numberLocal = labDOM.call('lab_flow_location_local', {value: 42}, null, localBody, null);
        equal(numberLocal.calls[0].args[2], '42', 'boxed numeric text cannot be mistaken for a scoped handle');
        labDOM.call('lab_flow_location_visibility', localCard, 'almanac');
        check(localCard.classList.contains('hidden'), 'local calendar hidden outside DateTime');
        labDOM.call('lab_flow_location_visibility', localCard, 'datetime');
        check(!localCard.classList.contains('hidden'), 'local calendar shown in DateTime');
        const select = document.createElement('select');
        const methods = {
            get rebuild() { return true; },
            get sync() { throw new Error('fallback getter read despite rebuild'); }
        };
        const selectPlan = labDOM.call('lab_flow_location_select', select, methods);
        equal(selectPlan.calls[0].service, 'locSelectRebuild', 'rebuild wins without probing fallback getter');
        const absent = labDOM.call('lab_flow_location_select', null, {
            get rebuild() { throw new Error('absent select queried'); }
        });
        equal((absent.calls || []).length, 0, 'absent select never probes method capabilities');
        const selectOrder = [];
        select.__marsRebuildRoundedSelect = function() {
            equal(this, select, 'rounded-select method receiver preserved');
            labDOM.call('lab_flow_location_picker', 0);
            selectOrder.push('rebuild');
        };
        select.__marsSyncRoundedSelect = () => selectOrder.push('sync');
        syncRoundedSelect(select);
        delete select.__marsRebuildRoundedSelect;
        syncRoundedSelect(select);
        syncRoundedSelect(null);
        equal(selectOrder.join(','), 'rebuild,sync', 'actual select wrappers execute outside native scopes');
        equal((labDOM.call('lab_flow_location_picker', 0).calls || []).length, 0, 'unchanged picker move is inert');
        equal(labDOM.call('lab_flow_location_picker', 1).calls[0].service, 'locRenderPicker', 'changed picker redraw');

        equal(step(39, {}).done, true, 'unallocated location kind terminates without effects');
        equal(step(27, {stage: 1, current: false}).value, null, 'stale restore explicitly returns null');
        equal(step(27, {stage: 1, current: true}).value, false, 'missing select differs from stale restore');
        check(Array.isArray(step(28, {}).value), 'missing population target returns an empty array');
        const option = Object.freeze({value: 'opaque|town'});
        const payloadFrame = {action: 2, option, value: ' historical key '};
        const payloadPlan = step(26, payloadFrame);
        equal(payloadPlan.wait, true, 'compatibility lookup waits for native forms service');
        equal(payloadPlan.calls[0].service, 'locForms', 'compatibility uses forms capability');
        equal(payloadPlan.calls[0].args[0].value, ' historical key ', 'town key is not client-normalised');
        equal(payloadPlan.calls[0].args[0].candidates[0], option.value, 'candidate remains opaque');
        equal(option.value, 'opaque|town', 'planning does not alter option');
        for (const index of [0, '0', false, null, undefined, -1]) {
            const result = step(26, {stage: 1, action: 2, response: {match_index: index}});
            equal(result.value, index === 0, 'native matching requires numeric zero');
        }
        const requests = [];
        requestLabForms = async request => {
            requests.push(request);
            return {text: '12:34:56', town: option, match_index: 0};
        };
        equal(await formatAlmanacTimeInput('123456'), '12:34:56', 'actual time wrapper runs native continuation');
        equal(await townValueParts('town-key'), option, 'actual town wrapper preserves result identity');
        equal(await townOptionMatchesValue(option, 'old-key'), true, 'actual compatibility wrapper');
        equal(await townOptionMatchesValue(null, 'old-key'), false, 'missing option performs no request');
        equal(requests.length, 3, 'only applicable wrappers request forms');
        equal(requests[0].action, 'time', 'time payload action');
        equal(requests[1].action, 'town', 'town payload action');

        const gate = new Promise(resolve => settle = resolve);
        pending = run(33, {mode: 'datetime', options: {refreshJurisdiction: true, refreshCoordinates: false}}, {
            currentMode: () => 'datetime',
            locRefreshDatetime: options => {
                equal(options.updateCoordinates, false, 'auto-refresh retains authored coordinates');
                events.push('refresh');
                return gate;
            },
            locSaveDatetime: () => events.push('save'),
            locEvaluateDatetime: options => {
                equal(options.skipHistoryUpdate, true, 'auto-evaluation owns final history update');
                events.push('evaluate');
            },
            updateHistoryButtons: () => events.push('history')
        });
        equal(events.join(','), 'refresh', 'persistence cannot overtake pending refresh');
        settle();
        await pending;
        pending = null;
        equal(events.join(','), 'refresh,save,evaluate,history', 'auto-evaluation service order');
        events.length = 0;
        await run(33, {mode: 'almanac', options: {}}, {
            currentMode: () => 'datetime',
            locClearAlmanac: () => events.push('clear')
        });
        equal(events.length, 0, 'inactive Almanac cannot invalidate its worksheet');

        const input = document.createElement('input');
        input.value = '123456';
        const context = {}, main = {};
        const timeServices = {
            locTimeSnapshot: () => ({element: input, authored: input.value, context, main}),
            locFormatTime: async () => '12:34:56',
            requestContext: () => context,
            locLatestMain: () => main,
            locTimeApply: (element, value) => {
                equal(element, input, 'formatting retains control identity');
                element.value = value;
                events.push('apply');
            },
            locStatus: error => events.push(error)
        };
        pending = run(34, {}, timeServices);
        input.value = 'author edited';
        await pending;
        pending = null;
        equal(input.value, 'author edited', 'late formatted text cannot replace authored edit');
        equal(events.length, 0, 'stale formatting has no visible effects');
        input.value = '123456';
        await run(34, {}, timeServices);
        equal(input.value, '12:34:56', 'current formatted value is published');
        events.length = 0;
        await run(34, {}, {...timeServices, locFormatTime: async () => { throw null; }});
        equal(events.join(','), 'null', 'even a null exception follows current-error recovery');
        events.length = 0;
        await run(34, {}, {
            ...timeServices,
            locFormatTime: async () => { throw new Error('old request'); },
            requestContext: () => ({})
        });
        equal(events.length, 0, 'stale formatting errors cannot change status');

        const request = {}, town = document.createElement('select');
        const state = {jurisdiction: 'GB-ENG', date: '2030-01-02'};
        const refreshServices = {
            locElements: () => ({town, jurisdiction: {value: 'GB-ENG'}, latitude: {value: '1'}, longitude: {value: '2'}}),
            locControls: () => ({}),
            locBegin: (operation, mode) => {
                equal(operation, 'datetimeLocation', 'refresh request operation');
                equal(mode, 'datetime', 'refresh request ownership');
                events.push('begin');
                return request;
            },
            locRead: () => state,
            locState: () => state,
            locPopulate: async () => events.push('populate'),
            requestCurrent: owner => {
                equal(owner, request, 'freshness checks retain request identity');
                return false;
            },
            locFinish: owner => {
                equal(owner, request, 'cleanup retains request identity');
                events.push('finish');
            }
        };
        await run(32, {mode: 'datetime', updateCoordinates: true}, refreshServices);
        equal(events.join(','), 'begin,populate,finish', 'stale population stops before coordinates and HTTP');
        events.length = 0;
        await run(32, {mode: 'datetime', updateCoordinates: true}, {
            ...refreshServices,
            locPopulate: async () => { throw new Error('location unavailable'); }
        });
        equal(events.join(','), 'begin,finish', 'refresh failure is quiet and still releases ownership');
        events.length = 0;
        await run(32, {mode: 'datetime', updateCoordinates: false}, {
            ...refreshServices,
            locApplyTown: () => events.push('town'),
            post: async (owner, payload) => {
                equal(owner, request, 'post retains request');
                equal(payload.jurisdiction, state.jurisdiction, 'post jurisdiction');
                equal(payload.date, state.date, 'post date');
                events.push('post');
                return {response: {ok: true}, data: {ok: true}};
            },
            requestCurrent: () => true,
            locContext: () => context,
            locResponse: (mode, data, owner, update, applied) => {
                equal(mode, 'datetime', 'response projection mode');
                equal(owner, context, 'response projection retains context');
                equal(update, false, 'response respects coordinate preservation');
                check(!applied, 'preserve-coordinate path does not report a selected replacement town');
                events.push('response');
            },
            locAcceptContext: owner => {
                equal(owner, context, 'updated context is committed after projection');
                events.push('accept');
            }
        });
        equal(events.join(','), 'begin,town,post,response,accept,finish', 'successful refresh order and cleanup');

        events.length = 0;
        await run(31, {button: {}}, {
            locBegin: () => request,
            locContext: () => context,
            native: entry => {
                equal(entry, 'lab_location_totality_prepare', 'stale totality never reaches finishing projection');
                return state;
            },
            locRestoreTotality: async () => null,
            locFinish: owner => {
                equal(owner, request, 'totality releases its original request');
                events.push('finish');
            }
        });
        equal(events.join(','), 'finish', 'stale totality does not save or evaluate');
        events.length = 0;
        const failure = new Error('render failed');
        let caught = null;
        try {
            await run(31, {button: {}}, {
                locBegin: () => request,
                locContext: () => { throw new Error('preparation failed'); },
                requestCurrent: () => true,
                locStatus: status => events.push(status),
                locError: () => { throw failure; },
                locFinish: () => events.push('finish'),
                rethrow: error => { throw error; }
            });
        } catch (error) {
            caught = error;
        }
        equal(caught, failure, 'error in totality recovery propagates the original error object');
        equal(events.join(','), 'Error,finish', 'failed recovery still performs exactly one cleanup');
    } finally {
        if (settle)
            settle();
        if (pending)
            await pending.catch(() => {});
        requestLabForms = savedRequest;
    }
};
