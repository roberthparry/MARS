/** Native binding request normalisation and accepted-edit projection; invoked sequentially after labReady. */
window.checkLabBindingSync = async function checkLabBindingSync() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Binding sync: ${message}`);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const plan = (body, bindings) => labDOM.call('lab_binding_sync_request', {body, bindings});
    const originalRecords = bindings => bindings.map(binding => ({
                                                         name: String(binding.name || ''),
                                                         kind: binding.kind || 'variable',
                                                         value: String(binding.value ?? binding.display ?? '?')
                                                     }));
    const sameRecords = (actual, expected, label) => {
        equal(actual.length, expected.length, `${label}: length`);
        for (let index = 0; index < expected.length; ++index) {
            equal(Object.hasOwn(actual, index), Object.hasOwn(expected, index), `${label}: hole ${index}`);
            if (!(index in expected))
                continue;
            for (const field of ['name', 'kind', 'value'])
                equal(actual[index][field], expected[index][field], `${label}: ${field} ${index}`);
        }
    };
    const deferred = () => {
        let resolve, reject;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const savedFunctions = new Map();
    const replace = (name, implementation) => {
        if (!savedFunctions.has(name))
            savedFunctions.set(name, window[name]);
        window[name] = implementation;
    };
    const saved = {
        fullText: labEditorState.fullText,
        displayText: labEditorState.displayText,
        editor: expr.value,
        refresh: expr.dataset.bindingRefreshValid,
        ready: expr.dataset.evaluationReady,
        variables: currentVariables,
        differentiable: currentDifferentiable,
        current: labRequests.current
    };
    const edited = '__binding_sync_α + c__', source = '__binding_sync_authored__';
    const cached = [edited, source].map(key => [key, labPresentationEditors.has(key), labPresentationEditors.get(key)]);
    const pending = [], tasks = [];
    try {
        for (const body of [undefined, null, false, 0, NaN, '', ' \u00a0\n ']) {
            const empty = plan(body, [null]);
            equal(empty.source, '', 'falsey/blank source suppresses invalid bindings');
            check(!empty.request && !empty.invalid, 'blank source needs no service');
        }
        const unread = new Proxy([], {
            get() {
                throw new Error('bindings must remain unevaluated');
            }
        });
        equal(plan('', unread).source, '', 'blank source does not read even an array length');
        for (const bindings of [undefined, null, false, 0, {}, {length: 2}, new Uint8Array([1]), []]) {
            const prepared = plan(' \u00a0opaque ∫ f(x) dx\n ', bindings);
            equal(prepared.source, 'opaque ∫ f(x) dx', 'source is browser-trimmed, never parsed');
            check(!prepared.request, 'non-arrays and empty arrays use preparation only');
        }
        const kind = {opaque: true};
        const records = [
            {}, {name: 0, kind: false, value: 0, display: 'ignored'},
            {name: false, kind: '', value: false, display: 'ignored'},
            {name: ' λ ', kind: null, value: '', display: 'ignored'},
            {name: '__proto__', kind, value: null, display: 'π/7'},
            {name: 'constructor', kind: 'constant', value: undefined, display: false}, {name: NaN, kind: 0, value: NaN},
            {name: 12, kind: 'Variable', display: 0}, {name: ['a', 'b'], value: {toString: () => 'a + b - c / d'}},
            {name: 'unset', value: null, display: undefined}, 0, false, 'primitive', Symbol('opaque')
        ];
        const requestPlan = plan('  body {not parsed}  ', records);
        equal(requestPlan.request.action, 'editor', 'native action');
        equal(requestPlan.request.operation, 'bindings', 'native operation');
        equal(requestPlan.request.mode, 'replace', 'native replacement policy');
        equal(requestPlan.request.text, 'body {not parsed}', 'opaque source');
        sameRecords(requestPlan.request.bindings, originalRecords(records), 'default/nullish conversion');
        equal(requestPlan.request.bindings[4].kind, kind, 'truthy kind is not stringified or copied');
        equal(records[3].name, ' λ ', 'input records are not trimmed or mutated');
        check(requestPlan.request.bindings[0] !== records[0], 'normalised entries are independent');

        const sparse = new Array(7);
        sparse[2] = {name: 'two', value: ''};
        sparse[5] = {name: 'five', display: null};
        sameRecords(plan('source', sparse).request.bindings, originalRecords(sparse), 'sparse array');
        sameRecords(plan('source', new Array(3)).request.bindings, new Array(3), 'all holes');
        const inherited = new Array(4);
        const prototype = Object.create(Array.prototype);
        Object.defineProperty(prototype, '1', {value: {name: 'inherited', value: 0}, configurable: true});
        Object.setPrototypeOf(inherited, prototype);
        sameRecords(plan('source', inherited).request.bindings, originalRecords(inherited), 'inherited array index');
        for (const value of [null, undefined])
            check(plan('source', [value]).invalid, 'present nullish record is rejected, not treated as a hole');
        const lazy = {name: 'lazy', value: ''};
        Object.defineProperty(lazy, 'display', {
            get() {
                throw new Error('display must remain unevaluated');
            }
        });
        equal(plan('source', [lazy]).request.bindings[0].value, '', 'nullish fallback is lazy');
        const many = Array.from({length: 8192}, (_, index) => ({name: `v${index}`, value: index}));
        const manyResult = plan('source', many).request.bindings;
        equal(manyResult.length, 8192, 'normalisation releases per-record handles');
        equal(manyResult[8191].value, '8191', 'large arrays retain their final record');

        const fixture = document.createElement('textarea');
        const bindings = [
            {name: ' α ', kind: 'variable', value: 'π/7'}, {name: 'c', kind: 'constant'}, {name: '', value: '?'},
            {name: 'β', kind: ''}, {name: 'α', kind: 'variable'}
        ];
        for (const flag of [undefined, null, false, 0, NaN, '', ' YES\u00a0', '\nNo ', true, [], ['yes'], {}]) {
            const data = {evaluation_ready: flag, differentiable: flag};
            const projected = labDOM.call('lab_binding_sync_apply', fixture, {displayText: 'α + c', bindings, data});
            equal(fixture.value, 'α + c', 'accepted display is copied exactly');
            equal(fixture.dataset.bindingRefreshValid, 'true', 'accepted binding refresh');
            equal(
                fixture.dataset.evaluationReady, String(flag || 'no').trim().toLowerCase() === 'yes' ? 'true' : 'false',
                'readiness semantics');
            equal(
                projected.differentiable, String(flag || 'yes').trim().toLowerCase() !== 'no',
                'differentiability semantics');
            equal(
                projected.variables.join('|'), variableNamesFromBindings(bindings).join('|'),
                'shared variable policy preserves discovery order and duplicates');
        }
        for (const data of [undefined, null, false, 0]) {
            const projected = labDOM.call('lab_binding_sync_apply', fixture, {displayText: '', bindings: [], data});
            equal(fixture.dataset.evaluationReady, 'false', 'absent native readiness defaults to no');
            equal(projected.differentiable, true, 'absent native differentiability defaults to yes');
        }

        const prepared = [], requested = [], renderedBindings = [], derivativeCalls = [], persisted = [];
        replace('prepareLabEditor', async text => {
            prepared.push(text);
        });
        replace('requestLabPresentation', async request => {
            requested.push(request);
            return {editor: {expression: 'native assembled expression'}};
        });
        equal(await expressionWithBindings('', [null]), '', 'blank host request returns immediately');
        equal(prepared.length, 0, 'blank host request skips preparation');
        equal(await expressionWithBindings('  source  ', []), 'source', 'empty bindings prepare and return source');
        equal(prepared[0], 'source', 'preparation receives normalised source');
        equal(await expressionWithBindings('source', sparse), 'native assembled expression', 'service owns assembly');
        sameRecords(requested[0].bindings, originalRecords(sparse), 'host service receives sparse records');
        for (const invalid of [null, undefined]) {
            let error;
            try {
                await expressionWithBindings('source', [invalid]);
            } catch (caught) {
                error = caught;
            }
            check(error instanceof TypeError, 'invalid mapped record rejects with TypeError');
        }
        equal(requested.length, 1, 'invalid records never reach the service');

        replace('renderVariableValues', values => {
            renderedBindings.push(values);
        });
        replace('renderDerivativeButtons', variables => {
            derivativeCalls.push({variables, differentiable: currentDifferentiable});
        });
        replace('saveWorksheetState', (...args) => {
            persisted.push(args);
        });
        replace('requestLabPresentation', request => {
            const wait = deferred();
            wait.request = request;
            pending.push(wait);
            return wait.promise;
        });
        labPresentationEditors.set(edited, {body: edited, wrapped: false, bindings: []});
        labPresentationEditors.set(source, {body: 'source body', wrapped: true, bindings: [{name: 'α', value: 'π/7'}]});
        let live = true;
        const owner = {};
        labRequests.current = request => request === owner && live;
        const data = {binding_values: bindings, evaluation_ready: ' YES ', differentiable: ' no '};
        const start = () => {
            const task = applyMarsBindingsToEditedExpression(edited, source, data, owner);
            tasks.push(task);
            return {task, wait: pending[pending.length - 1]};
        };
        expr.value = `  ${edited}\n`;
        const accepted = start();
        equal(accepted.wait.request.bindings[4].value, 'π/7', 'authored values remain selected by existing helpers');
        accepted.wait.resolve({editor: {expression: 'native full result'}});
        await accepted.task;
        equal(labEditorState.fullText, 'native full result', 'accepted native full source is retained');
        equal(labEditorState.displayText, edited, 'edited body remains display source');
        equal(expr.value, edited, 'whitespace-equivalent editor guard still accepts');
        equal(expr.dataset.evaluationReady, 'true', 'accepted readiness is projected');
        equal(currentDifferentiable, false, 'accepted differentiability reaches host state');
        equal(derivativeCalls[0].differentiable, false, 'derivative service sees projected metadata');
        equal(persisted[0][0], 'expression', 'accepted edit persistence mode');
        equal(persisted[0][1], 'native full result', 'persistence receives full native source');
        equal(persisted[0][2].debounce, true, 'persistence remains debounced');

        data.evaluation_ready = 'no';
        data.differentiable = 'yes';
        const stale = start();
        live = false;
        stale.wait.resolve({editor: {expression: 'stale identical body'}});
        await stale.task;
        equal(labEditorState.fullText, 'native full result', 'obsolete request with identical editor is rejected');
        equal(expr.dataset.evaluationReady, 'true', 'obsolete request cannot change readiness');
        equal(currentDifferentiable, false, 'obsolete request cannot change derivative metadata');
        equal(renderedBindings.length, 1, 'obsolete request does not rebuild controls');
        live = true;
        const changed = start();
        expr.value = 'newer editor text';
        changed.wait.resolve({editor: {expression: 'stale edited body'}});
        await changed.task;
        equal(expr.value, 'newer editor text', 'changed editor is not overwritten');
        equal(persisted.length, 1, 'stale edits do not persist');
        expr.value = edited;
        const fallback = start();
        fallback.wait.resolve({editor: {expression: ''}});
        await fallback.task;
        equal(labEditorState.fullText, edited, 'empty service assembly retains editor-body fallback');
        const failed = start();
        const failure = new Error('native service unavailable');
        failed.wait.reject(failure);
        let caught;
        try {
            await failed.task;
        } catch (error) {
            caught = error;
        }
        equal(caught, failure, 'service failures retain host propagation');
        equal(persisted.length, 2, 'failed service does not project or persist');
    } finally {
        labRequests.current = () => false;
        for (const wait of pending) wait.resolve({editor: {expression: ''}});
        await Promise.allSettled(tasks);
        for (const [name, original] of savedFunctions) window[name] = original;
        labRequests.current = saved.current;
        for (const [key, present, value] of cached) {
            if (present)
                labPresentationEditors.set(key, value);
            else
                labPresentationEditors.delete(key);
        }
        labEditorState.fullText = saved.fullText;
        labEditorState.displayText = saved.displayText;
        expr.value = saved.editor;
        for (const [field, value] of [['bindingRefreshValid', saved.refresh], ['evaluationReady', saved.ready]]) {
            if (value === undefined)
                delete expr.dataset[field];
            else
                expr.dataset[field] = value;
        }
        currentVariables = saved.variables;
        currentDifferentiable = saved.differentiable;
    }
};
