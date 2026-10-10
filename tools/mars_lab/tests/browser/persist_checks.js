/** Worksheet persistence schemas, browser adapters and stale timers against real C/WASM. */
window.checkLabPersistence = async function checkLabPersistence() {
    const native = labWire.exports();
    const check = (ok, message) => {
        if (!ok)
            throw new Error('Persistence: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const saved = {
        store: saveLabState,
        text: currentExpressionText,
        canonical: expressionWithSortedConstants,
        bounds: currentIntegratorBoundsText,
        cap: requestedIntegratorIntervalCap,
        operation: validMatrixOperation,
        timeout: window.setTimeout,
        clear: window.clearTimeout,
        set: Storage.prototype.setItem,
        now: Date.now,
        timestamp: lastExpressionUpdatedAt,
        editors: WORKSPACE_MODE_NAMES.map(mode => modeEditorText[mode]),
        operand: matrixOperand.value,
        prepare: prepareLabEditor,
        display: setExpressionEditor,
        integratorSource: expr.dataset.savedIntegratorExpression
    };
    const local = new Map(), sent = [], timers = new Map(), cancelled = [];
    let text = 'μ + σ', tick = 1000, next = 1, canonicalCalls = 0, storageFails = false;
    const expected = [
        ['expression', 'expression_updated_at'], ['equation', 'equation_updated_at'], ['diffequation'],
        ['matrix', 'matrix_operation', 'matrix_operand'],
        ['integrator_expression', 'integrator_bounds', 'integrator_interval_cap']
    ];
    const localKeys = [
        ['lastExpression', 'lastExpressionUpdatedAt'], ['lastEquation', 'lastEquationUpdatedAt'], ['lastDiffequation'],
        ['lastMatrix', 'lastMatrixOperation', 'lastMatrixOperand'],
        ['lastIntegratorExpression', 'lastIntegratorBounds', 'lastIntegratorIntervalCap']
    ];
    try {
        for (const timer of worksheetSaveTimers.values()) clearTimeout(timer);
        worksheetSaveTimers.clear();
        for (const mode of [5, 6, 7, -1]) {
            equal(native.lab_persist_count(mode), 0, 'invalid/calendar mode has no save schema');
            equal(native.lab_persist_source(mode, 0), -1, 'invalid source');
            equal(native.lab_persist_begin(mode, 1), 0, 'invalid mode cannot schedule');
            equal(native.lab_persist_take(mode, 1), 0, 'invalid mode cannot claim');
            equal(native.lab_persist_delay(mode, 1), 0, 'invalid mode cannot defer');
            equal(native.lab_persist_local(mode, 0, 1), 0, 'invalid mode cannot write storage');
        }
        for (let mode = 0; mode < 5; ++mode) {
            const fields = worksheetSaveSchema(mode);
            equal(fields.map(f => f.server).join(','), expected[mode].join(','), 'server field names');
            equal(
                fields.map(f => f.local).join(','), localKeys[mode].map(k => 'mars.exprLab.' + k).join(','),
                'local storage key compatibility');
            equal(native.lab_persist_source(mode, fields.length), -1, 'out of range field');
            equal(native.lab_persist_text_length(mode, 0, 2), 0, 'invalid label part');
            equal(native.lab_persist_text_length(mode, -1, 0), 0, 'invalid field label');
            equal(native.lab_persist_delay(mode, 1), mode < 2 ? 250 : 0, 'per-mode debounce');
            equal(native.lab_persist_local(mode, 0, 0), 0, 'empty local editor retained');
            equal(native.lab_persist_local(mode, 0, 1), 1, 'non-empty local editor saved');
        }
        equal(native.lab_persist_local(4, 1, 0), 0, 'blank bounds retained locally');
        equal(native.lab_persist_local(3, 2, 0), 1, 'blank operand clears locally');
        const first = native.lab_persist_begin(0, 1), second = native.lab_persist_begin(0, 1);
        const equation = native.lab_persist_begin(1, 0);
        equal(native.lab_persist_take(0, first), 0, 'superseded snapshot rejected');
        equal(native.lab_persist_take(1, second), 0, 'cross-mode token rejected');
        equal(native.lab_persist_take(0, second), 1, 'latest snapshot accepted');
        equal(native.lab_persist_take(0, second), 0, 'snapshot accepted only once');
        equal(native.lab_persist_take(1, equation), 1, 'blank equation independently accepted');

        for (let mode = 0; mode < 7; ++mode) {
            equal(native.lab_persist_restore(mode, 0, 0, 0), 0, 'empty editor not restored');
            equal(
                native.lab_persist_restore(mode, 1, 1, 0) !== 0, mode < 2,
                'partial editor copies rejected except expression/equation');
        }
        const prepared = [], displayed = [];
        prepareLabEditor = async value => prepared.push(value);
        setExpressionEditor = value => displayed.push(value);
        expressionWithSortedConstants = value => value;
        const restored = {
            expression: '  μ + σ  ',
            expression_updated_at: 1234,
            equation: '  x = 1  ',
            diffequation: 'partial...',
            matrix: 'complete matrix',
            integrator_expression: '  exp(x)  '
        };
        modeEditorText.diffequation = 'retained';
        await restoreWorksheetEditors(key => restored[key]);
        equal(prepared.join('|'), 'μ + σ|x = 1', 'only native binding-aware editors prepared');
        equal(displayed[0], 'μ + σ', 'server expression display trimmed');
        equal(lastExpressionUpdatedAt, 1234, 'server timestamp restored');
        equal(modeEditorText.diffequation, 'retained', 'partial editor does not overwrite useful text');
        equal(modeEditorText.matrix, 'complete matrix', 'complete matrix restored');
        equal(expr.dataset.savedIntegratorExpression, 'exp(x)', 'server integrator source retained');
        const localRestored = Object.fromEntries([0, 1, 2, 3, 4].flatMap(
            mode => worksheetSaveSchema(mode).map(field => [field.local, restored[field.server]])));
        localRestored['mars.exprLab.lastIntegratorExpression'] = '  local source  ';
        await restoreWorksheetEditors(key => localRestored[key], true);
        equal(displayed.at(-1), '  μ + σ  ', 'local editor whitespace retained');
        equal(modeEditorText.equation, '  x = 1  ', 'local equation whitespace retained');
        equal(modeEditorText.integrator, '  local source  ', 'local integrator restored');
        equal(expr.dataset.savedIntegratorExpression, 'exp(x)', 'local restore does not replace server source');
        prepareLabEditor = saved.prepare;
        setExpressionEditor = saved.display;

        saveLabState = (patch, options) => sent.push({patch, options});
        currentExpressionText = () => text;
        expressionWithSortedConstants = value => {
            ++canonicalCalls;
            return value;
        };
        currentIntegratorBoundsText = () => 'x = 0 .. 1';
        requestedIntegratorIntervalCap = () => 20000;
        validMatrixOperation = () => 'inverse';
        Date.now = () => ++tick;
        Storage.prototype.setItem = function(key, value) {
            if (storageFails)
                throw new Error('Storage disabled');
            local.set(key, value);
        };
        window.setTimeout = (callback, delay) => {
            const id = next++;
            timers.set(id, {callback, delay});
            return id;
        };
        window.clearTimeout = id => cancelled.push(id);

        saveWorksheetState('expression', '  μ + σ  ', {debounce: true});
        const old = timers.get(worksheetSaveTimers.get(0));
        equal(old.delay, 250, 'expression delay');
        equal(sent.length, 0, 'deferred save not sent early');
        equal(local.get('mars.exprLab.lastExpression'), 'μ + σ', 'UTF-8 text trimmed, not rewritten');
        equal(local.get(EXPRESSION_TIMESTAMP_STORAGE_KEY), '1001', 'local expression timestamp');
        equal(lastExpressionUpdatedAt, 1001, 'evaluation timestamp updated');
        saveWorksheetState('expression', 'new', {debounce: true});
        const currentId = worksheetSaveTimers.get(0), current = timers.get(currentId);
        old.callback();
        equal(sent.length, 0, 'stale timer cannot publish');
        equal(worksheetSaveTimers.get(0), currentId, 'stale callback cannot remove current timer');
        saveWorksheetState('expression', '   ', {debounce: true});
        equal(worksheetSaveTimers.get(0), currentId, 'blank expression preserves queued useful save');
        equal(modeEditorText.expression, 'new', 'blank expression retains editor');
        equal(local.get(EXPRESSION_TIMESTAMP_STORAGE_KEY), '1003', 'blank expression still timestamps local state');
        current.callback();
        equal(sent.at(-1).patch.expression, 'new', 'queued snapshot not changed by later blank');
        equal(sent.at(-1).patch.expression_updated_at, 1002, 'queued snapshot retains its timestamp');
        check(!worksheetSaveTimers.has(0), 'completed timer released');

        saveWorksheetState('expression', 'expression pending', {debounce: true});
        const expressionTimer = timers.get(worksheetSaveTimers.get(0));
        text = 'equation pending';
        saveWorksheetState('equation', undefined, {debounce: true});
        const equationTimer = timers.get(worksheetSaveTimers.get(1));
        saveWorksheetState('expression', 'immediate', {keepalive: true});
        equal(sent.at(-1).options.keepalive, true, 'immediate keepalive preserved');
        const count = sent.length;
        expressionTimer.callback();
        equal(sent.length, count, 'immediate save invalidates prior deferred save');
        equationTimer.callback();
        equal(sent.at(-1).patch.equation, 'equation pending', 'expression save does not cancel equation');
        equal(sent.at(-1).options.keepalive, false, 'timer save is not unload keepalive');
        text = '   ';
        saveWorksheetState('equation');
        equal(sent.at(-1).patch.equation, '', 'blank equation still sent');
        equal(local.get('mars.exprLab.lastEquation'), 'equation pending', 'blank equation retains useful local copy');
        equal(modeEditorText.equation, 'equation pending', 'blank equation retains editor');
        equal(canonicalCalls, 2, 'only equation needed native canonical metadata so far');

        matrixOperand.value = '   ';
        text = 'matrix';
        saveWorksheetState('matrix');
        equal(sent.at(-1).patch.matrix_operand, '', 'matrix operand cleared on server');
        equal(local.get('mars.exprLab.lastMatrixOperand'), '', 'matrix operand cleared locally');
        equal(sent.at(-1).patch.matrix_operation, 'inverse', 'matrix operation preserved');
        text = 'integrator';
        saveWorksheetState('integrator');
        equal(sent.at(-1).patch.integrator_interval_cap, 20000, 'interval cap remains a number');
        equal(local.get('mars.exprLab.lastIntegratorIntervalCap'), '20000', 'local cap is text');
        equal(sent.at(-1).patch.integrator_bounds, 'x = 0 .. 1', 'bounds captured');
        currentIntegratorBoundsText = () => '';
        saveWorksheetState('integrator');
        equal(sent.at(-1).patch.integrator_bounds, '', 'blank bounds sent to server');
        equal(local.get('mars.exprLab.lastIntegratorBounds'), 'x = 0 .. 1', 'local bounds retained');
        storageFails = true;
        text = 'differential equation';
        saveWorksheetState('diffequation');
        equal(sent.at(-1).patch.diffequation, text, 'storage failure still publishes server snapshot');
        equal(modeEditorText.diffequation, text, 'storage failure still retains editor');
        check(sent.every(item => item.patch.precision_bits === modePrecisionBits), 'precision included for every save');
        check(cancelled.length > 0, 'host cancels superseded timers');
    } finally {
        saveLabState = saved.store;
        currentExpressionText = saved.text;
        expressionWithSortedConstants = saved.canonical;
        currentIntegratorBoundsText = saved.bounds;
        requestedIntegratorIntervalCap = saved.cap;
        validMatrixOperation = saved.operation;
        Date.now = saved.now;
        Storage.prototype.setItem = saved.set;
        window.setTimeout = saved.timeout;
        window.clearTimeout = saved.clear;
        worksheetSaveTimers.clear();
        lastExpressionUpdatedAt = saved.timestamp;
        WORKSPACE_MODE_NAMES.forEach((mode, index) => modeEditorText[mode] = saved.editors[index]);
        matrixOperand.value = saved.operand;
        prepareLabEditor = saved.prepare;
        setExpressionEditor = saved.display;
        if (saved.integratorSource === undefined)
            delete expr.dataset.savedIntegratorExpression;
        else
            expr.dataset.savedIntegratorExpression = saved.integratorSource;
    }
};
