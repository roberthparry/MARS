/**
 * @file persist_checks.ts
 * @brief Worksheet persistence schema and stale-save regressions.
 *
 * Uses the real native planner with isolated browser storage and timers to check field mappings, save publication and stale callback handling.
 */

/** @brief Stored fields inspected by the persistence fixture. */
interface LabPersistRecord {
    [field: string]: unknown;
}

/** @brief Server publication captured with its original save options. */
interface LabPersistPublication {
    patch: LabPersistRecord;
    options: WorksheetSaveOptions;
}

/** @brief Browser timer retained for explicit current and stale callback checks. */
interface LabPersistTimer {
    callback(): void;
    delay: number | undefined;
}

/** @brief Check worksheet persistence schemas, browser adapters and stale timers against real C/WASM. */
window.checkLabPersistence = async function checkLabPersistence() {
    const native = labWire.exports();
    function check(ok: unknown, message: string): void {
        if (!ok)
            throw new Error('Persistence: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const saved = {
        store: WorksheetPersistence.persistence.publish,
        text: WorksheetBindings.editor.currentExpressionText,
        canonical: WorksheetBindings.editor.expressionWithSortedConstants,
        bounds: WorksheetWorkspace.integrator.currentIntegratorBoundsText,
        cap: WorksheetWorkspace.integrator.requestedIntegratorIntervalCap,
        operation: WorksheetPersistence.statePolicy.matrixOperation,
        timeout: window.setTimeout,
        clear: window.clearTimeout,
        set: Storage.prototype.setItem,
        now: Date.now,
        timestamp: WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt,
        editors: WorksheetWorkspace.WORKSPACE_MODE_NAMES.map(mode => WorksheetContext.modeEditorText[mode]),
        operand: WorksheetContext.matrixOperand.value,
        prepare: WorksheetRequests.presentation.prepareLabEditor,
        display: WorksheetBindings.controller.setExpressionEditor,
        integratorSource: WorksheetContext.expr.dataset.savedIntegratorExpression
    };
    const local = new Map<string, string>();
    const sent: LabPersistPublication[] = [];
    const timers = new Map<number, LabPersistTimer>();
    const cancelled: (number | undefined)[] = [];
    let text = 'μ + σ';
    let tick = 1000;
    let next = 1;
    let canonicalCalls = 0;
    let storageFails = false;
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
        for (const timer of WorksheetPersistence.persistence.scheduler.timers.values()) {
            clearTimeout(timer);
        }
        WorksheetPersistence.persistence.scheduler.timers.clear();
        for (const mode of [5, 6, 7, -1]) {
            equal(native.lab_persist_count(mode), 0, 'invalid/calendar mode has no save schema');
            equal(native.lab_persist_source(mode, 0), -1, 'invalid source');
            equal(native.lab_persist_begin(mode, 1), 0, 'invalid mode cannot schedule');
            equal(native.lab_persist_take(mode, 1), 0, 'invalid mode cannot claim');
            equal(native.lab_persist_delay(mode, 1), 0, 'invalid mode cannot defer');
            equal(native.lab_persist_local(mode, 0, 1), 0, 'invalid mode cannot write storage');
        }
        for (let mode = 0; mode < 5; ++mode) {
            const fields = (WorksheetPersistence.statePolicy.schema(mode) as LabWorksheetTestField[]);
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
        const first = native.lab_persist_begin(0, 1);
        const second = native.lab_persist_begin(0, 1);
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
        const prepared: string[] = [];
        const displayed: string[] = [];
        Reflect.set(WorksheetRequests.presentation, 'prepareLabEditor', async (value: string) => prepared.push(value));
        Reflect.set(WorksheetBindings.controller, 'setExpressionEditor', (value: string) => displayed.push(value));
        Reflect.set(WorksheetBindings.editor, 'expressionWithSortedConstants', (value: string) => value);
        const restored: LabPersistRecord = {
            expression: '  μ + σ  ',
            expression_updated_at: 1234,
            equation: '  x = 1  ',
            diffequation: 'partial...',
            matrix: 'complete matrix',
            integrator_expression: '  exp(x)  '
        };
        WorksheetContext.modeEditorText.diffequation = 'retained';
        await WorksheetPersistence.restoration.restoreEditors(key => restored[key]);
        equal(prepared.join('|'), 'μ + σ|x = 1', 'only native binding-aware editors prepared');
        equal(displayed[0], 'μ + σ', 'server expression display trimmed');
        equal(WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt, 1234, 'server timestamp restored');
        equal(WorksheetContext.modeEditorText.diffequation, 'retained', 'partial editor does not overwrite useful text');
        equal(WorksheetContext.modeEditorText.matrix, 'complete matrix', 'complete matrix restored');
        equal(WorksheetContext.expr.dataset.savedIntegratorExpression, 'exp(x)', 'server integrator source retained');
        const localRestored: LabPersistRecord = {};
        for (let mode = 0; mode < 5; ++mode) {
            const fields = WorksheetPersistence.statePolicy.schema(mode) as LabWorksheetTestField[];
            for (const field of fields) {
                localRestored[field.local] = restored[field.server];
            }
        }
        localRestored['mars.exprLab.lastIntegratorExpression'] = '  local source  ';
        await WorksheetPersistence.restoration.restoreEditors(key => localRestored[key], true);
        equal(displayed.at(-1), '  μ + σ  ', 'local editor whitespace retained');
        equal(WorksheetContext.modeEditorText.equation, '  x = 1  ', 'local equation whitespace retained');
        equal(WorksheetContext.modeEditorText.integrator, '  local source  ', 'local integrator restored');
        equal(WorksheetContext.expr.dataset.savedIntegratorExpression, 'exp(x)', 'local restore does not replace server source');
        Reflect.set(WorksheetRequests.presentation, 'prepareLabEditor', saved.prepare);
        Reflect.set(WorksheetBindings.controller, 'setExpressionEditor', saved.display);

        /** @brief Capture each server publication with the supplied options. */
        function recordPublication(patch: LabPersistRecord, options: WorksheetSaveOptions): number {
            return sent.push({patch, options});
        }

        Reflect.set(WorksheetPersistence.persistence, 'publish', recordPublication);
        Reflect.set(WorksheetBindings.editor, 'currentExpressionText', () => text);
        Reflect.set(WorksheetBindings.editor, 'expressionWithSortedConstants', function canonicalText(value: string) {
            ++canonicalCalls;
            return value;
        });
        Reflect.set(WorksheetWorkspace.integrator, 'currentIntegratorBoundsText', () => 'x = 0 .. 1');
        Reflect.set(WorksheetWorkspace.integrator, 'requestedIntegratorIntervalCap', () => 20000);
        Reflect.set(WorksheetPersistence.statePolicy, 'matrixOperation', () => 'inverse');
        Date.now = function nextTimestamp() {
            return ++tick;
        };
        Storage.prototype.setItem = function recordLocalValue(key, value) {
            if (storageFails)
                throw new Error('Storage disabled');
            local.set(key, value);
        };
        window.setTimeout = function captureTimer(callback, delay) {
            const id = next++;
            timers.set(id, {callback: callback as () => void, delay});
            return id;
        };
        window.clearTimeout = id => cancelled.push(id);

        WorksheetPersistence.persistence.saveWorksheet('expression', '  μ + σ  ', {debounce: true});
        const old = timers.get(WorksheetPersistence.persistence.scheduler.timers.get(0)!)!;
        equal(old.delay, 250, 'expression delay');
        equal(sent.length, 0, 'deferred save not sent early');
        equal(local.get('mars.exprLab.lastExpression'), 'μ + σ', 'UTF-8 text trimmed, not rewritten');
        equal(local.get(EXPRESSION_TIMESTAMP_STORAGE_KEY), '1001', 'local expression timestamp');
        equal(WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt, 1001, 'evaluation timestamp updated');
        WorksheetPersistence.persistence.saveWorksheet('expression', 'new', {debounce: true});
        const currentId = WorksheetPersistence.persistence.scheduler.timers.get(0);
        const current = timers.get(currentId!)!;
        old.callback();
        equal(sent.length, 0, 'stale timer cannot publish');
        equal(WorksheetPersistence.persistence.scheduler.timers.get(0), currentId, 'stale callback cannot remove current timer');
        WorksheetPersistence.persistence.saveWorksheet('expression', '   ', {debounce: true});
        equal(WorksheetPersistence.persistence.scheduler.timers.get(0), currentId, 'blank expression preserves queued useful save');
        equal(WorksheetContext.modeEditorText.expression, 'new', 'blank expression retains editor');
        equal(local.get(EXPRESSION_TIMESTAMP_STORAGE_KEY), '1003', 'blank expression still timestamps local state');
        current.callback();
        equal(sent.at(-1)!.patch.expression, 'new', 'queued snapshot not changed by later blank');
        equal(sent.at(-1)!.patch.expression_updated_at, 1002, 'queued snapshot retains its timestamp');
        check(!WorksheetPersistence.persistence.scheduler.timers.has(0), 'completed timer released');

        WorksheetPersistence.persistence.saveWorksheet('expression', 'expression pending', {debounce: true});
        const expressionTimer = timers.get(WorksheetPersistence.persistence.scheduler.timers.get(0)!)!;
        text = 'equation pending';
        WorksheetPersistence.persistence.saveWorksheet('equation', undefined, {debounce: true});
        const equationTimer = timers.get(WorksheetPersistence.persistence.scheduler.timers.get(1)!)!;
        WorksheetPersistence.persistence.saveWorksheet('expression', 'immediate', {keepalive: true});
        equal(sent.at(-1)!.options.keepalive, true, 'immediate keepalive preserved');
        const count = sent.length;
        expressionTimer.callback();
        equal(sent.length, count, 'immediate save invalidates prior deferred save');
        equationTimer.callback();
        equal(sent.at(-1)!.patch.equation, 'equation pending', 'expression save does not cancel equation');
        equal(sent.at(-1)!.options.keepalive, false, 'timer save is not unload keepalive');
        text = '   ';
        WorksheetPersistence.persistence.saveWorksheet('equation');
        equal(sent.at(-1)!.patch.equation, '', 'blank equation still sent');
        equal(local.get('mars.exprLab.lastEquation'), 'equation pending', 'blank equation retains useful local copy');
        equal(WorksheetContext.modeEditorText.equation, 'equation pending', 'blank equation retains editor');
        equal(canonicalCalls, 2, 'only equation needed native canonical metadata so far');

        WorksheetContext.matrixOperand.value = '   ';
        text = 'matrix';
        WorksheetPersistence.persistence.saveWorksheet('matrix');
        equal(sent.at(-1)!.patch.matrix_operand, '', 'matrix operand cleared on server');
        equal(local.get('mars.exprLab.lastMatrixOperand'), '', 'matrix operand cleared locally');
        equal(sent.at(-1)!.patch.matrix_operation, 'inverse', 'matrix operation preserved');
        text = 'integrator';
        WorksheetPersistence.persistence.saveWorksheet('integrator');
        equal(sent.at(-1)!.patch.integrator_interval_cap, 20000, 'interval cap remains a number');
        equal(local.get('mars.exprLab.lastIntegratorIntervalCap'), '20000', 'local cap is text');
        equal(sent.at(-1)!.patch.integrator_bounds, 'x = 0 .. 1', 'bounds captured');
        Reflect.set(WorksheetWorkspace.integrator, 'currentIntegratorBoundsText', () => '');
        WorksheetPersistence.persistence.saveWorksheet('integrator');
        equal(sent.at(-1)!.patch.integrator_bounds, '', 'blank bounds sent to server');
        equal(local.get('mars.exprLab.lastIntegratorBounds'), 'x = 0 .. 1', 'local bounds retained');
        storageFails = true;
        text = 'differential equation';
        WorksheetPersistence.persistence.saveWorksheet('diffequation');
        equal(sent.at(-1)!.patch.diffequation, text, 'storage failure still publishes server snapshot');
        equal(WorksheetContext.modeEditorText.diffequation, text, 'storage failure still retains editor');
        check(sent.every(item => item.patch.precision_bits === WorksheetContext.modePrecisionBits), 'precision included for every save');
        check(cancelled.length > 0, 'host cancels superseded timers');
    } finally {
        Reflect.set(WorksheetPersistence.persistence, 'publish', saved.store);
        Reflect.set(WorksheetBindings.editor, 'currentExpressionText', saved.text);
        Reflect.set(WorksheetBindings.editor, 'expressionWithSortedConstants', saved.canonical);
        Reflect.set(WorksheetWorkspace.integrator, 'currentIntegratorBoundsText', saved.bounds);
        Reflect.set(WorksheetWorkspace.integrator, 'requestedIntegratorIntervalCap', saved.cap);
        Reflect.set(WorksheetPersistence.statePolicy, 'matrixOperation', saved.operation);
        Date.now = saved.now;
        Storage.prototype.setItem = saved.set;
        window.setTimeout = saved.timeout;
        window.clearTimeout = saved.clear;
        WorksheetPersistence.persistence.scheduler.timers.clear();
        WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt = saved.timestamp;
        for (let index = 0; index < WorksheetWorkspace.WORKSPACE_MODE_NAMES.length; ++index) {
            const mode = WorksheetWorkspace.WORKSPACE_MODE_NAMES[index];
            WorksheetContext.modeEditorText[mode] = saved.editors[index];
        }
        WorksheetContext.matrixOperand.value = saved.operand;
        Reflect.set(WorksheetRequests.presentation, 'prepareLabEditor', saved.prepare);
        Reflect.set(WorksheetBindings.controller, 'setExpressionEditor', saved.display);
        if (saved.integratorSource === undefined)
            delete WorksheetContext.expr.dataset.savedIntegratorExpression;
        else
            WorksheetContext.expr.dataset.savedIntegratorExpression = saved.integratorSource;
    }
};
