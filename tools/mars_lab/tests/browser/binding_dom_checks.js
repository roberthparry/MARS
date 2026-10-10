/** Binding and integrator DOM regressions against the C/WASM presentation controller. */
window.checkLabBindingDom = async function checkLabBindingDom() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Binding DOM: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const deferred = () => {
        let resolve;
        const promise = new Promise(accept => resolve = accept);
        return {promise, resolve};
    };
    const fixture = document.createElement('div');
    fixture.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:450px';
    const root = document.createElement('div'), rows = document.createElement('div');
    const fallback = document.createElement('input');
    fixture.append(root, rows, fallback);
    document.body.appendChild(fixture);
    const savedFunctions = new Map();
    const replace = (name, implementation) => {
        if (!savedFunctions.has(name))
            savedFunctions.set(name, window[name]);
        window[name] = implementation;
    };
    const saved = {
        nodes: Array.from(variableValues.childNodes),
        parent: variableValues.parentNode,
        next: variableValues.nextSibling,
        hidden: variableValues.classList.contains('hidden'),
        index: variableValues.labBindingIndex,
        rows: Array.from(integratorBoundStack.childNodes),
        rowParent: integratorBoundStack.parentNode,
        rowNext: integratorBoundStack.nextSibling,
        cache: bindingValueCache,
        kinds: currentBindingKinds,
        pending: pendingExpressionBindingCommit,
        references: integratorReferenceMetadata,
        fullText: labEditorState.fullText,
        refresh: expr.dataset.bindingRefreshValid,
        ready: expr.dataset.evaluationReady,
        editor: expr.value,
        run: labRequests.run,
        current: labRequests.current,
        timeout: window.setTimeout,
        active: document.activeElement
    };
    const nativeEvent = (target, type, extra = {}) => labDOM.call('lab_binding_event', root, {target, type, ...extra});
    const keys = (target, key, extra = {}) => {
        const event = new KeyboardEvent('keydown', {key, bubbles: true, cancelable: true, ...extra});
        target.dispatchEvent(event);
        return event.defaultPrevented;
    };
    try {
        const catalogue = labDOM.call('lab_binding_row_names');
        equal(catalogue.length, 108, 'C owns the complete row-name catalogue');
        equal(catalogue.slice(0, 9).join(','), 'x,y,z,t,u,v,w,r,s', 'preferred row-name ordering');
        equal(catalogue[107], 'x99', 'numbered row-name range');
        equal(integratorDefaultVariableName(catalogue.map(name => ({name}))), 'x', 'exhaustion retains x fallback');
        equal(
            integratorDefaultVariableName([{name: ' x '}, {name: 'y'}, {name: 'X'}]), 'z',
            'occupancy trims names but preserves case');
        const normalised = sanitizeIntegratorRow({kind: 'Free', name: '', lo: ' α + β ', hi: 'α - β'}, 'μ');
        equal(normalised.kind, 'bound', 'only exact free kind selects a free parameter');
        equal(normalised.name, 'μ', 'missing row name uses supplied fallback');
        equal(normalised.lo, ' α + β ', 'row normalisation does not rewrite bounds');
        const preparedRows = labDOM.call('lab_binding_rows_prepare', [{name: 'x'}, {name: ''}, {name: 'y'}, {}]);
        equal(
            preparedRows.map(row => row.name).join(','), 'x,y,y,z', 'fallback names follow original authored prefixes');
        equal(integratorFallbackRows()[0].hi, '1', 'standard bound upper default');
        equal(integratorBlankRows()[0].hi, '', 'blank bound remains indefinite');
        equal(labDOM.call('lab_binding_rows_prepare', null)[0].lo, '0', 'empty render input receives standard default');
        const manyRows = Array.from({length: 256}, (_, index) => ({name: `opaque ${index}`, lo: 'π/7'}));
        equal(labDOM.call('lab_binding_rows_prepare', manyRows).length, 256, 'normalisation scopes temporary handles');

        const planRows = [
            {kind: 'bound', name: 'x', lo: '', hi: ''}, {kind: 'bound', name: 'unused', lo: '', hi: ''},
            {kind: 'free', name: '__proto__', lo: '', hi: ''}
        ];
        const metadata = labDOM.call('lab_binding_rows_metadata', 'opaque expression', {
            references_valid: true,
            rows: [
                {name: 'x', referenced: true}, {name: 'unused', referenced: false},
                {name: '__proto__', referenced: false}
            ]
        });
        const plan = labDOM.call('lab_binding_rows_plan', planRows, metadata, 'opaque expression');
        equal(plan.rows.length, 1, 'current native references filter inactive rows');
        equal(plan.rows[0], planRows[0], 'selection preserves row identity');
        equal(
            labDOM.call('lab_binding_rows_plan', planRows, metadata, 'changed expression').rows.length, 3,
            'stale reference metadata conservatively retains every row');
        equal(
            labDOM.call('lab_binding_rows_reference', metadata, 'opaque expression', 'missing'), 1,
            'unknown names are retained');
        equal(
            labDOM.call('lab_binding_rows_reference', metadata, 'opaque expression', '__proto__'), 0,
            'prototype-like identifiers use exact Map membership');
        metadata.valid = false;
        equal(
            labDOM.call('lab_binding_rows_plan', planRows, metadata, 'opaque expression').rows.length, 3,
            'invalid native reference analysis cannot discard rows');
        const fallbackPlan = labDOM.call('lab_binding_rows_plan', [], null, '');
        equal(fallbackPlan.bounds[0].name, 'x', 'empty planner supplies a bound');
        equal(fallbackPlan.rows[0].hi, '1', 'empty planner supplies the full default');
        equal(labDOM.call('lab_binding_rows_plan', [...manyRows, {}], null, ''), null, 'oversized plan is rejected');
        const authoredRow = {kind: 'bound', name: 'x', lo: 'a+b', hi: 'a-b', extra: 'retained'};
        const edited = planIntegratorRowEdit([authoredRow], 0, 1);
        equal(edited[0].kind, 'free', 'toggle makes a free parameter');
        equal(edited[0].lo, '', 'toggle clears inactive bounds');
        equal(edited[0].extra, 'retained', 'editing preserves other metadata');
        equal(authoredRow.lo, 'a+b', 'editing does not mutate input rows');
        equal(edited[1].name, 'y', 'last-bound toggle appends the next available bound');
        equal(labDOM.call('lab_binding_rows_edit', [authoredRow], 0, 3), null, 'sole bound cannot be removed');
        const free = {kind: 'free', name: ' μ ', lo: '', hi: ''};
        const resultBound = {kind: 'bound', name: 'z', lo: '0', hi: 'π'};
        const merged = labDOM.call(
            'lab_binding_rows_merge', [authoredRow, free],
            {bounds: [resultBound], binding_values: [{name: 'μ', kind: 'variable'}]});
        equal(merged[0], resultBound, 'merge installs the native result bound');
        equal(merged[1], free, 'merge retains matching free parameter identity');
        equal(
            labDOM
                .call(
                    'lab_binding_rows_merge', [authoredRow, free],
                    {bounds: [resultBound], binding_values: [{name: 'μ', kind: 'constant'}]})
                .length,
            1, 'constant bindings do not retain free parameters');
        equal(labDOM.call('lab_binding_rows_merge', [authoredRow], {}), null, 'absent result bounds request no update');
        equal(
            labDOM.call('lab_binding_rows_merge', [free], {bounds: manyRows, binding_values: [{name: 'μ'}]}), false,
            'merged row limit includes retained parameters');

        const bindingRecords = [{name: ' x ', kind: 'variable'}, {name: 'μ'}, {name: 'c', kind: 'constant'}];
        equal(variableNamesFromBindings(bindingRecords).join(','), 'x,μ', 'variable identities exclude constants');
        const selectedBindings = labDOM.call('lab_binding_select', 4, bindingRecords, new Set(['x']));
        equal(selectedBindings.length, 2, 'integration bounds are not editable parameters');
        equal(selectedBindings[0], bindingRecords[1], 'selection retains native binding identity');
        const editorInputs = {
            text: '  opaque full source  ',
            metadata: {body: 'native body', bindings: bindingRecords},
            body: null,
            bodyMetadata: null,
            bindings: null,
            ready: null
        };
        for (let mode = 0; mode < 7; ++mode) {
            const selection = labDOM.call('lab_binding_editor', mode, editorInputs, new Set(['x']));
            equal(selection.fullText, 'opaque full source', 'editor retains trimmed opaque source');
            equal(selection.displayText, 'native body', 'editor uses native body metadata');
            equal(selection.bindings.length, mode === 0 ? 0 : mode === 4 ? 2 : 3, 'per-mode binding selection');
            equal(selection.refresh, mode === 0, 'only expression mode requests missing readiness analysis');
        }
        const explicitEditor = labDOM.call(
            'lab_binding_editor', 0,
            {...editorInputs, bindings: [], body: '', bodyMetadata: {body: ''}, ready: ' YES '}, null);
        equal(explicitEditor.bindings.length, 0, 'explicit empty evaluated bindings override native bindings');
        equal(explicitEditor.displayText, '', 'explicit empty body does not revive native body');
        equal(explicitEditor.refresh, false, 'provided readiness avoids a second analysis');
        equal(expr.dataset.evaluationReady, 'true', 'readiness normalisation belongs to C');
        labDOM.call('lab_binding_editor', 0, editorInputs, null);
        check(!expr.hasAttribute('data-evaluation-ready'), 'absent readiness clears stale DOM metadata');

        const goalSource = {
            bindings: [
                {name: 'x'}, {name: 'y'}, {name: 'u'}, {name: 'c', kind: 'constant'}, {name: '__proto__'},
                {name: 'missing'}
            ]
        };
        const solved = {
            bindings: [
                {name: 'x', value: 'solved x'}, {name: 'y', value: 'old y'}, {name: 'y', value: 'π/7'},
                {name: 'u', value: 'stale', unset: true}, {name: 'c', value: 'constant replacement'},
                {name: '__proto__', value: 'symbolic'}
            ]
        };
        const starts = {x: 'provided x', y: 'provided y', u: 'keep unset', c: 'keep constant', missing: 'keep missing'};
        const cache = new Map([['x', 'authored x'], ['c', 'cached constant']]);
        const goal = labDOM.call('lab_binding_goal_starts', goalSource, solved, starts, cache);
        equal(goal.x, 'authored x', 'cached authored value wins over solved and supplied starts');
        equal(goal.y, 'π/7', 'last solved binding wins over supplied start');
        equal(goal.u, 'keep unset', 'unset solved binding does not replace supplied start');
        equal(goal.c, 'keep constant', 'source constants are excluded from start replacement');
        equal(goal.missing, 'keep missing', 'missing solved value preserves supplied start');
        check(Object.hasOwn(goal, '__proto__') && goal.__proto__ === 'symbolic', 'goal names are safe own data keys');
        equal(Object.getPrototypeOf(goal), Object.prototype, 'goal identifiers cannot change the result prototype');
        equal(starts.x, 'provided x', 'goal precedence does not mutate supplied starts');
        const withoutMetadata = labDOM.call('lab_binding_goal_starts', null, solved, starts, cache);
        equal(withoutMetadata.x, starts.x, 'missing source metadata preserves supplied starts');
        check(withoutMetadata !== starts, 'goal starts are copied');
        const goalDefaults = labDOM.call(
            'lab_binding_goal_prepare', {
                goal_expression: 'opaque native goal',
                starts: {x: 'native x', zero: 'native zero', blank: 'native blank'}
            },
            {x: 'provided', zero: 0, blank: '', exact: '0'});
        equal(goalDefaults.expression, 'opaque native goal', 'goal expression is copied without interpretation');
        equal(goalDefaults.start.x, 'provided', 'truthy provided start precedes native default');
        equal(goalDefaults.start.zero, 'native zero', 'legacy numeric-zero start accepts native default');
        equal(goalDefaults.start.exact, '0', 'symbolic zero remains a supplied start');
        equal(goalDefaults.start.blank, 'native blank', 'blank start accepts native default');

        const bindings = [
            {name: 'c10', kind: 'constant', value: '10'},
            {name: '[μ]', kind: 'variable', value: ' sin(π/7) + √2 ', display: 'approximation'},
            {name: 'c2', kind: 'constant', value: '?'}, {name: 'z', value: 'NaN'},
            {name: 'É2', kind: 'constant', value: '2'}, {name: 'e2', kind: 'constant', value: '3'},
            {name: 'c2', kind: 'constant', value: '4'}
        ];
        const compare = (left, right) => {
            const a = String(left?.name || left || ''), b = String(right?.name || right || '');
            return a.localeCompare(b, undefined, {numeric: true, sensitivity: 'base'}) || a.localeCompare(b);
        };
        const expected = [bindings[1], bindings[3], ...bindings.filter(item => item.kind === 'constant').sort(compare)];
        const rendered = labDOM.call('lab_binding_render', root, bindings);
        const inputs = Array.from(root.querySelectorAll('.binding-value-input'));
        equal(
            inputs.map(input => input.dataset.bindingName).join('|'), expected.map(item => item.name).join('|'),
            'variables retain discovery order and constants use stable browser collation');
        equal(inputs[0].value, 'sin(π/7) + √2', 'exact symbolic value wins over abbreviated display');
        equal(inputs[0].title, inputs[0].value, 'full value remains the tooltip');
        equal(inputs[0].getAttribute('aria-label'), 'Value of μ', 'display brackets removed from accessible label');
        equal(inputs[0].dataset.bindingName, '[μ]', 'native bracketed identity preserved');
        equal(inputs[0].placeholder, ' ', 'blank placeholder retains clear-button CSS behaviour');
        equal(inputs[0].autocomplete, 'off', 'browser completion disabled');
        equal(inputs[0].spellcheck, false, 'browser spelling disabled');
        equal(inputs[1].value, '', 'NaN display is empty');
        equal(new Map(rendered.values).get('[μ]'), inputs[0].value, 'cache receives complete authored value');
        equal(new Map(rendered.kinds).get('z'), 'variable', 'missing kind defaults to variable');
        equal(root.querySelector('.variable-toggle').textContent, 'Constant', 'variable toggle caption');
        equal(
            root.querySelector('.constant-value-box .variable-toggle').textContent, 'Variable',
            'constant toggle caption');
        equal(
            root.querySelector('.binding-value-clear').getAttribute('aria-label'), 'Clear μ', 'clear accessible label');
        for (const left of ['c2', 'c10', 'É2', 'e2', '[μ]', '', 0, 12, {name: 'é10'}]) {
            for (const right of ['c2', 'c10', 'e2', '[μ]', 2]) {
                equal(
                    Math.sign(labDOM.call('lab_binding_compare', [left, right])), Math.sign(compare(left, right)),
                    'name comparator preserves browser semantics');
            }
        }
        for (const [name, label] of [['[μ]', 'μ'], ['[]', ''], ['[x', '[x'], ['x]', 'x]'], ['[[x]]', '[x]'], [0, '']])
            equal(bindingDisplayName(name), label, 'display labels do not interpret identifiers');
        for (const [value, unset] of [
                 ['', true], [' ? ', true], ['nAn', true], ['NaN(x)', false], ['π/7', false], [0, true]])
            equal(
                Boolean(labDOM.call('lab_binding_unset', [value])), unset,
                'only existing unset sentinels are recognised');
        equal(
            labDOM.call('lab_binding_value', {value: 0, display: 'fallback'}), 'fallback',
            'legacy truthy value precedence');
        equal(labDOM.call('lab_binding_value', {value: '1/3', display: '0.333…'}), '1/3', 'full value stays symbolic');
        const toggled = labDOM.call('lab_binding_toggle', {name: ' [μ] ', kind: ' constant '});
        equal(toggled.name, '[μ]', 'kind changes retain trimmed native identity');
        equal(toggled.nextKind, 'variable', 'kind changes use the existing trimmed-kind policy');

        inputs[0].value = '  exp(α) / 3  ';
        equal(labDOM.call('lab_binding_normalised', inputs[0]), 'exp(α) / 3', 'normalisation only trims authored text');
        const visible = labDOM.call('lab_binding_visible', root);
        equal(visible[0].value, visible[0].display, 'visible value and display agree');
        const snapshot = labDOM.call('lab_binding_snapshot', inputs);
        equal(snapshot[0].text, '  exp(α) / 3  ', 'commit snapshot retains untrimmed text');
        equal(snapshot[0].input, inputs[0], 'commit snapshot retains input identity');
        equal(labDOM.call('lab_binding_snapshot_current', snapshot), 1, 'unchanged snapshot is current');
        inputs[0].value = 'newer';
        equal(labDOM.call('lab_binding_snapshot_current', snapshot), 0, 'typing invalidates pending snapshot');
        inputs[0].value = snapshot[0].text;
        const committed = labDOM.call('lab_binding_committed', snapshot);
        equal(inputs[0].value, 'exp(α) / 3', 'accepted commit projects exact trimmed value');
        equal(committed[0][1], 'exp(α) / 3', 'accepted commit exports exact cache value');
        equal(committed[1][1], null, 'accepted unset commit requests cache deletion');
        const detachedInput = document.createElement('input');
        detachedInput.value = 'π/7';
        const detachedSnapshot = labDOM.call('lab_binding_snapshot', [detachedInput]);
        equal(
            labDOM.call('lab_binding_snapshot_current', detachedSnapshot), 1,
            'snapshot ownership does not introduce a new connectivity condition');

        const untouched = {name: 'u', value: 'native'};
        const overlay = labDOM.call(
            'lab_binding_authored', [{name: '__proto__', kind: 'constant', extra: 'kept'}, {name: 'x'}, untouched],
            [{name: '__proto__', value: 'π', display: 'π'}, {name: 'x', value: 'old'}],
            [{name: ' x ', value: 'exact', display: 'short'}]);
        equal(overlay[0].value, 'π', 'prototype-like names remain ordinary binding identities');
        equal(overlay[0].extra, 'kept', 'overlay retains other native metadata');
        equal(overlay[1].value, 'exact', 'visible authored values take precedence');
        equal(overlay[2], untouched, 'unmatched discovered object identity preserved');

        const original = inputs[0].closest('.variable-value-box').labBindingCard;
        inputs[0].focus();
        inputs[0].value = 'discard this';
        equal(nativeEvent(inputs[0], 'keydown', {key: 'Escape'}).prevent, 1, 'Escape suppresses browser default');
        equal(inputs[0].value, 'sin(π/7) + √2', 'Escape restores the rendered value, not a later commit');
        check(document.activeElement !== inputs[0], 'Escape blurs after releasing native handles');
        equal(
            nativeEvent(inputs[0], 'keydown', {key: 'Enter', ctrlKey: true}).action, 'evaluate',
            'Ctrl+Enter evaluates');
        equal(
            nativeEvent(inputs[0], 'keydown', {key: 'Enter', metaKey: true}).action, 'evaluate',
            'Meta+Enter evaluates');
        equal(nativeEvent(inputs[0], 'keydown', {key: 'a'}), null, 'ordinary typing has no native action');
        const clear = root.querySelector('.binding-value-clear');
        equal(nativeEvent(clear, 'pointerdown').prevent, 1, 'clear pointerdown prevents premature blur');
        equal(nativeEvent(clear, 'click').action, 'clear', 'clear requests an asynchronous commit');
        equal(inputs[0].value, '', 'clear empties input before its commit');
        equal(document.activeElement, inputs[0], 'clear retains input focus');
        equal(
            nativeEvent(root.querySelector('.variable-copy'), 'click').card.message, 'Copied μ',
            'copy status uses native label');
        equal(
            nativeEvent(root.querySelector('.variable-toggle'), 'click').card.binding, bindings[1],
            'toggle retains binding object');
        labDOM.call('lab_binding_render', root, bindings);
        labDOM.call('lab_binding_refocus', root, original, expr);
        equal(
            document.activeElement, root.querySelector('.binding-value-input'),
            'clear refocus finds replacement by name and kind');
        labDOM.call('lab_binding_clear', root);
        check(root.classList.contains('hidden') && !root.childNodes.length, 'empty binding region is hidden');
        labDOM.call('lab_binding_refocus', root, original, fallback);
        equal(document.activeElement, fallback, 'missing replacement falls back to editor');
        const many =
            Array.from({length: 180}, (_, index) => ({name: 'c' + (180 - index), kind: 'constant', value: '1/3'}));
        labDOM.call('lab_binding_render', root, many);
        equal(root.children.length, many.length, 'rendering releases temporary handles for every card and merge');
        equal(root.querySelector('input').dataset.bindingName, 'c1', 'large constant group retains numeric ordering');

        const rowData = [{kind: 'bound', name: '[α]', lo: 'a+b', hi: 'a-b'}, {kind: 'free', name: 'μ', lo: '', hi: ''}];
        labDOM.call('lab_binding_integrator_render', rows, rowData);
        const capturedRows = labDOM.call('lab_binding_rows_read', rows);
        equal(capturedRows.length, 2, 'row capture stays inside the supplied root');
        equal(capturedRows[0].lo, 'a+b', 'row capture preserves opaque lower bound');
        equal(capturedRows[1].kind, 'free', 'row capture preserves free parameter kind');
        equal(labDOM.call('lab_binding_rows_read', null)[0].hi, '1', 'missing row root uses the standard default');
        equal(rows.children.length, 2, 'integrator rows built in C');
        equal(
            rows.querySelector('.integrator-bound-toggle').title, 'Leave [α] free', 'integrator keeps native row name');
        check(rows.querySelector('.integrator-bound-remove').disabled, 'sole bound cannot be removed');
        check(rows.children[1].querySelector('[data-integrator-lower]').disabled, 'free lower bound is disabled');
        check(!rows.children[1].querySelector('.integrator-bound-remove').disabled, 'free row can be removed');
        const lower = rows.querySelector('[data-integrator-lower]'),
              upper = rows.querySelector('[data-integrator-upper]');
        upper.focus();
        upper.value = 'latest upper';
        labDOM.call('lab_binding_integrator_update', rows, [{name: '[β]', lo: 'normalised lower', hi: 'stale upper'}]);
        equal(lower.value, 'normalised lower', 'inactive prepared field updated in place');
        equal(upper.value, 'latest upper', 'focused field protected from blur response');
        equal(document.activeElement, upper, 'preparation preserves focus');
        const rowEvent = (target, type, extra = {}) =>
            labDOM.call('lab_binding_integrator_event', rows, {target, type, ...extra});
        equal(rowEvent(upper, 'keydown', {key: 'Escape'}).prevent, 1, 'integrator Escape handled in C');
        equal(upper.value, 'a-b', 'integrator Escape restores original authored bound');
        equal(rowEvent(lower, 'keydown', {key: 'Enter', ctrlKey: true}).action, '', 'integrator Ctrl+Enter only blurs');
        equal(rowEvent(lower, 'input').action, 'invalidate', 'typing invalidates row preparation');
        equal(rowEvent(lower, 'change').action, 'prepare', 'changed row requests native preparation');
        for (const [selector, operation] of [
                 ['.integrator-bound-toggle', 1], ['.integrator-bound-add', 2], ['.integrator-bound-remove', 3]]) {
            const action = rowEvent(rows.children[1].querySelector(selector), 'click');
            equal(action.operation, operation, 'row edit operation policy');
            equal(action.index, 1, 'row edit retains correct native index');
        }

        let history = 0, evaluations = 0, copied = '', status = '', source = 'source', mode = 'expression';
        replace('updateHistoryButtons', () => ++history);
        replace('evaluateFromKeyboard', () => ++evaluations);
        replace('setStatus', text => status = text);
        replace('flashCopyButton', () => {});
        replace('saveCurrentModeEditorState', () => {});
        replace('currentMode', () => mode);
        replace('currentExpressionText', () => source);
        replace('writeClipboardText', async text => copied = text);
        window.setTimeout = () => 0;
        fixture.append(variableValues, integratorBoundStack);
        renderVariableValues([{name: '[μ]', kind: 'variable', value: 'π/7'}]);
        const input = variableValues.querySelector('input');
        const card = input.closest('.variable-value-box').labBindingCard;
        input.dispatchEvent(new Event('input'));
        equal(history, 1, 'thin host adapter handles even non-bubbling input events');
        const evaluationCommit = deferred();
        replace('commitBindingInput', () => evaluationCommit.promise);
        check(keys(input, 'Enter', {ctrlKey: true}), 'keyboard adapter prevents Ctrl+Enter default');
        equal(evaluations, 0, 'keyboard evaluation awaits binding commit');
        evaluationCommit.resolve(false);
        await evaluationCommit.promise;
        await Promise.resolve();
        equal(evaluations, 1, 'unchanged commit still permits keyboard evaluation');
        input.value = 'fresh clipboard text';
        await labBindingActions.copy({card});
        equal(copied, 'fresh clipboard text', 'clipboard reads current input rather than rendered cache');
        equal(status, 'Copied μ', 'clipboard completion uses native status text');
        replace('writeClipboardText', async () => {
            throw new Error('clipboard rejected');
        });
        await labBindingActions.copy({card});
        check(status.includes('clipboard rejected'), 'clipboard errors remain handled by async adapter');

        const clearing = deferred();
        replace('queueBindingInputCommit', () => clearing.promise);
        const clearAction = labDOM.call(
            'lab_binding_event', variableValues,
            {target: variableValues.querySelector('.binding-value-clear'), type: 'click'});
        const clearingDone = labBindingActions.clear(clearAction);
        renderVariableValues([{name: '[μ]', kind: 'variable', value: ''}]);
        clearing.resolve(true);
        await clearingDone;
        equal(
            document.activeElement, variableValues.querySelector('input'),
            'async clear focuses rerendered replacement');

        // Control each response explicitly; no network timing or timers are needed.
        labRequests.run = async (_operation, _mode, callback) => callback({});
        let requestCurrent = true, callbackCurrent = true, response;
        labRequests.current = () => requestCurrent;
        replace('requestLabPresentation', () => response.promise);
        for (const reason of ['typing', 'editor', 'source', 'request', 'callback']) {
            renderVariableValues([{name: 'x', kind: 'variable', value: '1/3'}]);
            const committing = variableValues.querySelector('input');
            expr.value = 'editor';
            source = 'source';
            requestCurrent = callbackCurrent = true;
            response = deferred();
            const before = labEditorState.fullText;
            const pending = commitLabBindingValues([committing], () => callbackCurrent);
            if (reason === 'typing')
                committing.value = 'newer';
            else if (reason === 'editor')
                expr.value = 'newer editor';
            else if (reason === 'source')
                source = 'newer source';
            else if (reason === 'request')
                requestCurrent = false;
            else
                callbackCurrent = false;
            response.resolve({editor: {expression: 'accepted'}});
            equal(await pending, false, reason + ' rejects stale binding commit');
            equal(labEditorState.fullText, before, reason + ' leaves source state untouched');
        }
        requestCurrent = callbackCurrent = true;
        source = 'source';
        renderVariableValues([{name: 'x', kind: 'variable', value: '1/3'}]);
        const committing = variableValues.querySelector('input');
        committing.value = '  π/7  ';
        response = deferred();
        const successful = commitLabBindingValues([committing]);
        response.resolve({editor: {expression: 'accepted'}});
        equal(await successful, true, 'current binding commit accepted');
        equal(committing.value, 'π/7', 'accepted commit retains exact mathematics');
        equal(bindingValueCache.get('x'), 'π/7', 'accepted commit updates authored cache');

        let queuedCommits = 0;
        replace('commitBindingInput', async () => ++queuedCommits);
        const queue = savedFunctions.get('queueBindingInputCommit');
        pendingExpressionBindingCommit = Promise.resolve();
        const wrongMode = queue(committing);
        mode = 'equation';
        equal(await wrongMode, false, 'queued commit stops after mode changes');
        equal(queuedCommits, 0, 'stale queued mode makes no commit');
        mode = 'expression';
        const disconnected = queue(committing);
        committing.closest('.variable-value-box').remove();
        equal(await disconnected, false, 'queued commit stops after input disconnects');
        equal(queuedCommits, 0, 'disconnected queued input makes no commit');

        renderIntegratorRows(rowData);
        const rowInput = integratorBoundStack.querySelector('[data-integrator-lower]');
        let preparation = deferred();
        replace('refreshIntegratorForms', () => preparation.promise);
        const preparing = prepareIntegratorBindingInput(rowInput);
        rowInput.value = 'newer authored bound';
        preparation.resolve([{kind: 'bound', name: 'old', lo: 'stale', hi: 'stale'}]);
        await preparing;
        equal(rowInput.value, 'newer authored bound', 'late preparation cannot overwrite newer authored bound');
        preparation = deferred();
        const detached = prepareIntegratorBindingInput(rowInput);
        renderIntegratorRows(rowData);
        preparation.resolve([{kind: 'bound', name: 'old', lo: 'stale', hi: 'stale'}]);
        await detached;
        equal(
            integratorBoundStack.querySelector('[data-integrator-lower]').value, 'a+b',
            'disconnected input cannot install a delayed form response');
        replace('commitVisibleBindingInputs', async () => false);
        const detachedItem = integratorBoundStack.firstElementChild;
        const editing = editIntegratorRow(detachedItem, 0, 1);
        detachedItem.remove();
        await editing;
        equal(integratorBoundStack.children.length, 1, 'detached row edit stops after awaited commit');
    } finally {
        for (const [name, implementation] of savedFunctions) window[name] = implementation;
        labRequests.run = saved.run;
        labRequests.current = saved.current;
        window.setTimeout = saved.timeout;
        variableValues.replaceChildren(...saved.nodes);
        variableValues.classList.toggle('hidden', saved.hidden);
        variableValues.labBindingIndex = saved.index;
        integratorBoundStack.replaceChildren(...saved.rows);
        saved.parent.insertBefore(variableValues, saved.next);
        saved.rowParent.insertBefore(integratorBoundStack, saved.rowNext);
        bindingValueCache = saved.cache;
        currentBindingKinds = saved.kinds;
        pendingExpressionBindingCommit = saved.pending;
        integratorReferenceMetadata = saved.references;
        labEditorState.fullText = saved.fullText;
        expr.value = saved.editor;
        if (saved.refresh === undefined)
            delete expr.dataset.bindingRefreshValid;
        else
            expr.dataset.bindingRefreshValid = saved.refresh;
        if (saved.ready === undefined)
            delete expr.dataset.evaluationReady;
        else
            expr.dataset.evaluationReady = saved.ready;
        fixture.remove();
        saved.active?.focus();
    }
    checkLabIntegratorBindingState();
};

/** Integrator response policy uses native metadata only and defers state accessors beyond the DOM scope. */
function checkLabIntegratorBindingState() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Integrator binding state: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const names = plan => plan.calls.map(call => call.service).join(',');
    const authored = 'integrator binding fixture authored';
    const canonical = ' { native body | const c = π/7 } ';
    const editors = new Map([
        [authored, Object.freeze({expression: canonical, wrapped: true})],
        [canonical.trim(), Object.freeze({expression: canonical, wrapped: true})]
    ]);
    const bound = new Set(['x']);
    const bindings = Object.freeze([
        Object.freeze({name: ' x ', kind: 'variable', value: 'opaque bound'}),
        Object.freeze({name: 'μ', kind: 'variable', value: '  α + β  '}),
        Object.freeze({name: 'c', kind: 'constant', value: 'π/7'}),
        Object.freeze({name: 'x', kind: 'constant', value: '?'}),
        Object.freeze({name: '__proto__', kind: 'variable', value: 'native value'})
    ]);
    const response = Object.freeze(
        {binding_expression: '  ' + authored + '\n', expression: ' native body ', binding_values: bindings});
    const prepare = (data, fallback = '', cache = editors, excluded = bound) =>
        labDOM.call('lab_binding_integrator_state', data, fallback, cache, excluded);
    const savedEditor = expr.value, savedModeText = modeEditorText.integrator;
    const serviceNames =
        ['setExpressionEditor', 'renderVariableValues', 'clearVariableValues', 'currentIntegratorBoundNames'];
    const savedServices = new Map(serviceNames.map(name => [name, window[name]]));
    const savedCache = new Map([...editors.keys()].map(
        key => [key, {present: labPresentationEditors.has(key), value: labPresentationEditors.get(key)}]));
    try {
        expr.value = '  authored body fallback  ';
        let plan = prepare(response);
        equal(
            names(plan), 'setExpressionEditor,setIntegratorBindingExpression',
            'wrapped editor then saved integrator source');
        equal(plan.calls[0].args[0], canonical, 'server-canonical expression remains byte-for-byte opaque');
        equal(plan.calls[0].args[2], 'native body', 'response body is trimmed without mathematical rewriting');
        const selected = plan.calls[0].args[1];
        equal(selected.length, 3, 'bound names excluded for both variable and constant records');
        check(
            selected[0] === bindings[1] && selected[1] === bindings[2] && selected[2] === bindings[4],
            'editable records retain discovery order and original identity');
        equal(selected[0].value, '  α + β  ', 'authored binding whitespace is not normalised');
        equal(plan.calls[1].args[0], canonical, 'saved integrator expression matches editor source exactly');
        equal(expr.value, '  authored body fallback  ', 'preparation cannot project the editor before services');
        equal(modeEditorText.integrator, savedModeText, 'preparation cannot overwrite mode storage');
        prepare(null);
        check(selected[0] === bindings[1], 'returned records survive handle-table reuse');
        selected.pop();
        equal(prepare(response).calls[0].args[1].length, 3, 'returned binding arrays do not alias later selections');
        equal(bindings.length, 5, 'selection does not mutate frozen native arrays');

        for (const absent of [undefined, null, false, 0, '']) {
            plan = prepare({binding_expression: absent, expression: absent, binding_values: []}, authored);
            equal(
                names(plan), 'setExpressionEditor,clearVariableValues,setIntegratorBindingExpression',
                'empty wrapped parameter list explicitly clears after editor projection');
            equal(plan.calls[0].args[0], canonical, 'falsey response uses fallback binding expression');
            equal(plan.calls[0].args[2], 'authored body fallback', 'falsey response body uses current editor');
        }
        plan = prepare({binding_expression: authored, expression: ' \n\t ', binding_values: []});
        equal(plan.calls[0].args[2], null, 'truthy blank body becomes null without falling back to editor');
        expr.value = '';
        equal(prepare({binding_expression: authored}).calls[0].args[2], null, 'missing body and editor become null');
        equal(
            names(prepare({binding_expression: '   '}, authored)), 'clearVariableValues',
            'truthy blank response overrides rather than borrows fallback expression');
        for (const values of [undefined, null, {}, 'not an array', {0: bindings[1], length: 1}]) {
            equal(
                names(prepare({binding_values: values})), 'clearVariableValues',
                'only actual binding arrays are editable');
        }
        plan = prepare({binding_expression: '{ unknown | c = π/7 }', binding_values: bindings});
        equal(names(plan), 'renderVariableValues', 'authored braces do not imply recognised binding metadata');
        equal(plan.calls[0].args[0][1], bindings[2], 'non-editor path preserves constants');
        plan = prepare(response, '', new Map([[authored, editors.get(authored)]]));
        equal(names(plan), 'renderVariableValues', 'canonical expression requires its own wrapped metadata lookup');
        plan = prepare(response, '', new Map([[authored, {expression: '', wrapped: true}]]));
        equal(plan.calls[0].args[0], authored, 'falsey canonical source retains trimmed authored cache key');
        plan =
            prepare(response, '', new Map([[authored, editors.get(authored)], [canonical.trim(), {wrapped: false}]]));
        equal(names(plan), 'renderVariableValues', 'unwrapped canonical metadata does not change editor');
        const opaqueCache = new Map([['7', {expression: '7', wrapped: true}]]);
        equal(
            prepare({binding_expression: 7, expression: 9}, '', opaqueCache).calls[0].args[2], '9',
            'truthy scalar values retain browser String conversion semantics');
        const many = Array.from({length: 1600}, (_, index) => ({name: 'parameter ' + index, value: 'π/7'}));
        const large = prepare({binding_values: many}, '', new Map(), new Set());
        equal(large.calls[0].args[0].length, many.length, 'large responses respect scoped temporary-handle budget');
        equal(large.calls[0].args[0][1599], many[1599], 'large selection retains actual records rather than handles');

        // Execute the public wrapper through its local service registry; no application services are registered
        // globally.
        const calls = [];
        for (const name of serviceNames.slice(0, 3)) {
            window[name] = (...args) => {
                prepare(null);
                calls.push({name, args, modeText: modeEditorText.integrator});
            };
        }
        window.currentIntegratorBoundNames = () => bound;
        for (const [key, value] of editors) labPresentationEditors.set(key, value);
        modeEditorText.integrator = 'previous integrator source';
        equal(applyIntegratorBindingState(response), undefined, 'public wrapper remains a synchronous void helper');
        equal(
            calls.map(call => call.name).join(','), 'setExpressionEditor',
            'wrapper executes the selected editor service');
        equal(
            calls[0].modeText, 'previous integrator source',
            'source storage follows rather than precedes editor service');
        equal(modeEditorText.integrator, canonical, 'wrapper assigns through the original workspace accessor');
        check(
            typeof Object.getOwnPropertyDescriptor(modeEditorText, 'integrator').set === 'function',
            'native plan does not replace the live mode accessor with a data property');
        calls.length = 0;
        applyIntegratorBindingState({binding_expression: authored, binding_values: [bindings[0]]});
        equal(
            calls.map(call => call.name).join(','), 'setExpressionEditor,clearVariableValues',
            'all-bound parameters clear controls after setting editor');
        calls.length = 0;
        modeEditorText.integrator = 'keep unrecognised source';
        applyIntegratorBindingState({binding_values: [bindings[1]]});
        equal(calls[0].name, 'renderVariableValues', 'unrecognised expression only renders editable parameters');
        equal(
            modeEditorText.integrator, 'keep unrecognised source', 'non-editor branch leaves stored source unchanged');
        calls.length = 0;
        applyIntegratorBindingState(null);
        equal(calls[0].name, 'clearVariableValues', 'empty response clears only binding controls');
        window.setExpressionEditor = () => {
            throw new Error('fixture editor failure');
        };
        let failed = false;
        try {
            applyIntegratorBindingState(response);
        } catch (error) {
            failed = error.message === 'fixture editor failure';
        }
        check(
            failed && modeEditorText.integrator === 'keep unrecognised source',
            'failed editor projection cannot proceed to source storage');
    } finally {
        for (const [name, service] of savedServices) window[name] = service;
        for (const [key, entry] of savedCache) {
            if (entry.present)
                labPresentationEditors.set(key, entry.value);
            else
                labPresentationEditors.delete(key);
        }
        expr.value = savedEditor;
        modeEditorText.integrator = savedModeText;
    }
}
