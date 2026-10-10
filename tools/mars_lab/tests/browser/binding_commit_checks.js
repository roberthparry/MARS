/** Accepted binding commit and kind-toggle plans, cache projection and asynchronous ownership checks. */
window.checkLabBindingCommit = async function checkLabBindingCommit() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Binding commit: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const names = plan => plan.calls.map(call => call.service).join(',');
    const request = (mode, source, snapshot) => labDOM.call('lab_binding_commit_request', mode, source, snapshot);
    const plan = (mode, operation, source, updated, context) =>
        labDOM.call('lab_binding_commit_plan', mode, operation, source, updated, context);
    const source = '  opaque native { μ = π/7 }  ', updated = '  changed native { μ = ? }  ';
    const first = Object.freeze({name: 'μ', kind: 'variable', value: 'π/7'});
    const second = Object.freeze({name: '__proto__', kind: 'constant', value: '?'});
    const snapshot = Object.freeze(
        [Object.freeze({binding: first}), Object.freeze({binding: second}), Object.freeze({binding: first})]);
    for (let mode = 0; mode < 7; ++mode) {
        const payload = request(mode, source, snapshot);
        equal(payload.action, 'editor', 'binding payload action');
        equal(payload.operation, 'bindings', 'binding payload operation');
        equal(payload.mode, 'merge', 'binding payload merge policy');
        equal(payload.text, source, 'source is copied without mathematical interpretation or trimming');
        equal(payload.unset_constants, mode !== 0, 'only Expression preserves constant values');
        equal(payload.bindings.length, 3, 'duplicate bindings are not removed');
        check(
            payload.bindings[0] === first && payload.bindings[1] === second && payload.bindings[2] === first,
            'binding identities and duplicate order are retained');
        equal(request(mode, source, []), null, 'empty snapshots do not create requests');
    }
    for (const mode of [-1, 7, 2147483647])
        equal(request(mode, source, snapshot), null, 'invalid request mode is inert');
    const sparse = new Array(5);
    sparse[1] = snapshot[0];
    sparse[4] = snapshot[1];
    const mapped = request(0, source, sparse).bindings;
    equal(mapped.length, 5, 'sparse mapping retains array length');
    for (let index = 0; index < 5; ++index) equal(index in mapped, index in sparse, 'sparse mapping retains holes');
    equal(mapped[1], first, 'sparse binding identity retained');
    const inherited = new Array(2), prototype = Object.create(Array.prototype);
    Object.defineProperty(prototype, '1', {value: snapshot[1]});
    Object.setPrototypeOf(inherited, prototype);
    const inheritedMap = request(0, source, inherited).bindings;
    equal(inheritedMap[1], second, 'Array.map semantics include inherited populated indices');
    check(!(0 in inheritedMap), 'unrelated inherited-array holes remain holes');

    const editor = document.createElement('textarea');
    editor.value = '  authored body μ  ';
    editor.dataset.bindingRefreshValid = 'pending';
    const isCurrent = () => true;
    const context = Object.freeze({snapshot, editor, isCurrent});
    const forbidden = Object.freeze({
        get snapshot() {
            throw new Error('Unexpected snapshot read');
        },
        get editor() {
            throw new Error('Unexpected editor read');
        },
        get isCurrent() {
            throw new Error('Unexpected callback read');
        }
    });
    for (let mode = 0; mode < 7; ++mode) {
        for (const operation of [0, 1]) {
            const accepted = plan(mode, operation, source, updated, context);
            const expected = mode === 0 ?
                operation === 0 ? 'source,cache,updateHistoryButtons,saveCurrentModeEditorState' :
                                  'applyMarsBindingExpression' :
                'applyUpdatedBindingExpression,refreshVariableValuesFromEditor,updateHistoryButtons,' +
                    'saveCurrentModeEditorState';
            equal(names(accepted), expected, 'accepted mode/operation service order');
            equal(accepted.result, true, 'accepted change reports true');
            equal(accepted.wait, mode === 0 && operation === 1, 'only Expression kind projection is awaited');
            if (mode === 0 && operation === 0) {
                equal(accepted.calls[0].args[0], updated, 'accepted full source retains exact bytes');
                equal(accepted.calls[1].args[0], snapshot, 'cache service receives actual snapshot');
                equal(accepted.calls[1].args[1], editor, 'cache service receives actual editor');
            } else if (mode === 0) {
                equal(accepted.calls[0].args[1], 'authored body μ', 'kind projection trims visible editor body');
            } else {
                const refresh = accepted.calls[1];
                equal(refresh.args.length, operation === 0 ? 1 : 0, 'refresh argument omission is preserved');
                if (operation === 0)
                    equal(refresh.args[0], isCurrent, 'value commit retains the real ownership callback');
            }
            const unchanged = plan(mode, operation, source, source, forbidden);
            equal(names(unchanged), '', 'unchanged response performs no effects or context reads');
            equal(unchanged.result, false, 'unchanged response reports false');
            equal(unchanged.wait, false, 'unchanged response does not await');
        }
        for (const empty of ['', null, undefined, false, 0])
            equal(plan(mode, 0, source, empty, forbidden).result, false, 'falsey value update is rejected lazily');
        equal(plan(mode, 1, source, '', context).result, true, 'changed empty kind update remains accepted');
    }
    for (const [mode, operation] of [[-1, 0], [7, 0], [0, -1], [0, 2]])
        equal(plan(mode, operation, source, updated, forbidden), null, 'invalid plan selection is inert');
    plan(0, 0, source, updated, {
        snapshot,
        editor,
        get isCurrent() {
            throw new Error('Expression commit must not read refresh callback');
        }
    });
    plan(0, 1, source, updated, {
        editor,
        get snapshot() {
            throw new Error('Toggle must not read commit snapshot');
        },
        get isCurrent() {
            throw new Error('Toggle must not read refresh callback');
        }
    });
    plan(3, 1, source, updated, forbidden);
    equal(editor.value, '  authored body μ  ', 'planning cannot normalise the editor');
    equal(editor.dataset.bindingRefreshValid, 'pending', 'planning cannot mark bindings committed');
    const effects = [];
    const failure = new Error('binding commit service fixture');
    let caught;
    try {
        labDOM.services(plan(0, 0, source, updated, context), {
            source: value => {
                effects.push(value);
            },
            cache: () => {
                throw failure;
            },
            updateHistoryButtons: () => effects.push('history'),
            saveCurrentModeEditorState: () => effects.push('save')
        });
    } catch (error) {
        caught = error;
    }
    equal(caught, failure, 'service failure propagates unchanged');
    equal(effects.length, 1, 'cache failure stops history and persistence');
    equal(effects[0], updated, 'source effect precedes cache projection');

    // Detached controls avoid changing the real worksheet or triggering browser input events.
    const entry = (name, value) => {
        const input = document.createElement('input');
        input.value = 'not yet normalised';
        input.title = 'old title';
        return Object.freeze({input, binding: Object.freeze({name, value})});
    };
    const committed = Object.freeze([
        entry('μ', 'π/7'), entry('unset', '?'), entry('__proto__', 'exact'), entry('μ', 'later duplicate'),
        entry(' spaced ', 'opaque value')
    ]);
    const cache = new Map([['unset', 'old'], ['unrelated', 'retain'], ['spaced', 'different exact key']]);
    const writes = [];
    const observe = name => {
        equal(editor.dataset.bindingRefreshValid, 'true', 'refresh flag precedes cache writes');
        for (const item of committed) {
            equal(
                item.input.value, item.binding.value === '?' ? '' : item.binding.value,
                'every DOM input is normalised before the first cache mutation');
            equal(item.input.title, item.binding.value, 'every tooltip is projected before cache mutation');
        }
        writes.push(name);
    };
    cache.set = (name, value) => {
        observe(name);
        return Map.prototype.set.call(cache, name, value);
    };
    cache.delete = name => {
        observe(name);
        return Map.prototype.delete.call(cache, name);
    };
    labDOM.call('lab_binding_committed_cache', committed, editor, cache);
    equal(writes.join('|'), 'μ|unset|__proto__|μ| spaced ', 'cache updates follow exact snapshot order');
    equal(cache.get('μ'), 'later duplicate', 'last duplicate wins');
    check(!cache.has('unset'), 'unset sentinel deletes existing cache entry');
    equal(cache.get('__proto__'), 'exact', 'prototype-like names remain ordinary Map keys');
    equal(cache.get(' spaced '), 'opaque value', 'cache uses exact names without trimming');
    equal(cache.get('spaced'), 'different exact key', 'normalisation cannot collapse cache keys');
    equal(cache.get('unrelated'), 'retain', 'unrelated cache entries remain');
    const legacy = committed.map(item => entry(item.binding.name, item.binding.value));
    const pairs = labDOM.call('lab_binding_committed', legacy);
    legacy.forEach((item, index) => {
        equal(item.input.value, committed[index].input.value, 'legacy and cache exports share input projection');
        equal(item.input.title, committed[index].input.title, 'legacy and cache exports share tooltip projection');
        equal(pairs[index][0], item.binding.name, 'legacy export retains exact cache name');
        equal(pairs[index][1], item.binding.value === '?' ? null : item.binding.value, 'legacy unset representation');
    });
    const many = Object.freeze(Array.from({length: 1600}, (_, index) => entry('p' + index, index % 2 ? '?' : 'π/7')));
    const manyCache = new Map(many.map(item => [item.binding.name, 'old']));
    const largeRequest = request(4, source, many);
    equal(largeRequest.bindings.length, 1600, 'large request releases temporary handles');
    labDOM.call('lab_binding_committed_cache', many, editor, manyCache);
    equal(manyCache.size, 800, 'large cache projection updates and deletes all records');
    equal(many[1598].input.value, 'π/7', 'large projection reaches final populated control');
    equal(many[1599].input.value, '', 'large projection reaches final unset control');
    equal(largeRequest.bindings[1599], many[1599].binding, 'request records survive later handle-table reuse');

    // Exercise the actual kind-toggle adapter, retaining its asynchronous ownership guards.
    const savedBody = expr.value, originals = new Map(), events = [];
    const replace = (name, callback) => {
        if (!originals.has(name))
            originals.set(name, window[name]);
        window[name] = callback;
    };
    const deferred = () => {
        let resolve;
        const promise = new Promise(accept => {
            resolve = accept;
        });
        return {promise, resolve};
    };
    let mode = 'matrix', liveSource = source;
    const pending = [];
    try {
        replace('currentMode', () => mode);
        replace('currentExpressionText', () => liveSource);
        for (const name
                 of ['applyUpdatedBindingExpression', 'refreshVariableValuesFromEditor', 'updateHistoryButtons',
                     'saveCurrentModeEditorState'])
            replace(name, (...args) => events.push({name, args}));
        replace('replaceBindingKindInExpression', async () => updated);
        expr.value = '  visible body  ';
        await toggleBindingKind(first);
        equal(
            events.map(event => event.name).join(','),
            'applyUpdatedBindingExpression,refreshVariableValuesFromEditor,updateHistoryButtons,' +
                'saveCurrentModeEditorState',
            'real non-Expression toggle follows ordered native effects');
        equal(events[1].args.length, 0, 'real toggle omits refresh ownership argument');
        for (const reason of ['mode', 'editor', 'source']) {
            mode = 'matrix';
            liveSource = source;
            expr.value = 'visible body';
            const gate = deferred();
            replace('replaceBindingKindInExpression', () => gate.promise);
            events.length = 0;
            const task = toggleBindingKind(first);
            pending.push({gate, task});
            if (reason === 'mode')
                mode = 'equation';
            else if (reason === 'editor')
                expr.value = 'newer visible body';
            else
                liveSource = 'newer authored source';
            gate.resolve(updated);
            await task;
            equal(events.length, 0, 'stale ' + reason + ' cannot apply toggle completion');
        }
        mode = 'expression';
        liveSource = source;
        expr.value = '  visible body  ';
        replace('replaceBindingKindInExpression', async () => '');
        const gate = deferred(), entered = deferred();
        replace('applyMarsBindingExpression', (...args) => {
            events.push({name: 'applyMarsBindingExpression', args});
            entered.resolve();
            return gate.promise;
        });
        events.length = 0;
        let finished = false;
        const task = toggleBindingKind(first).then(() => {
            finished = true;
        });
        pending.push({gate, task});
        await Promise.race([
            entered.promise, task.then(() => {
                throw new Error('Binding commit: Expression toggle skipped its projection');
            })
        ]);
        equal(finished, false, 'Expression toggle awaits accepted projection');
        equal(events[0].args[0], '', 'changed empty kind update reaches real adapter');
        equal(events[0].args[1], 'visible body', 'Expression toggle captures trimmed editor body');
        gate.resolve();
        await task;
        equal(events.length, 1, 'Expression projection owns its own history and persistence');
    } finally {
        for (const item of pending) {
            item.gate.resolve(source);
            try {
                await item.task;
            } catch (_) { /* Preserve the original assertion during fixture cleanup. */
            }
        }
        for (const [name, callback] of originals) window[name] = callback;
        expr.value = savedBody;
    }
};
