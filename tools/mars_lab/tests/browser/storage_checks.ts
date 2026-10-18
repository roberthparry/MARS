/**
 * @file storage_checks.ts
 * @brief Native saved-state policy and guarded restoration regressions.
 *
 * Exercises control validation and asynchronous history restoration against the storage controller, including superseded ownership guards.
 */

/** @brief Native editor restoration result. */
interface LabStorageEditor {
    text: string;
    flags: number;
}

/** @brief Native recovery choice and its saved timestamp. */
interface LabStorageRecovery {
    text: string;
    updatedAt: number;
}

/** @brief Stored fields accessed by native restoration and recovery. */
interface LabStorageRecord {
    [field: string]: unknown;
}

/** @brief Check saved-state policies and guarded restoration against the C/WASM storage controller. */
window.checkLabStorage = async function checkLabStorage() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Storage: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const native = labWire.exports();
    const existingEquationControl = document.getElementById('equationVariable') as HTMLInputElement | null;
    const equationControl = existingEquationControl || document.createElement('input');
    if (!existingEquationControl) {
        equationControl.id = 'equationVariable';
        equationControl.hidden = true;
    }
    const controls = [
        equationControl, WorksheetContext.matrixOperation, WorksheetContext.matrixOperand,
        WorksheetContext.integratorIntervalCap, WorksheetContext.expr
    ];
    const saved = {
        values: controls.map(node => node.value),
        precision: WorksheetWorkspace.WORKSPACE_MODE_NAMES.map((_, index) => native.lab_workspace_precision(index)),
        prepare: WorksheetRequests.presentation.prepareLabEditor,
        bounds: WorksheetWorkspace.integrator.restoreIntegratorBoundsText,
        calendar: WorksheetLocations.calendarActions.restoreCalendarHistory,
        updated: WorksheetBindings.controller.applyUpdatedBindingExpression,
        source: WorksheetBindings.controller.clearExpressionSource,
        bindings: WorksheetBindings.panel.clearVariableValues
    };
    const option = document.createElement('option');
    option.value = 'storage-fixture';
    WorksheetContext.matrixOperation.appendChild(option);
    /** @brief Apply one native storage field using either local or server policy. */
    function control(field: number, value: unknown, local = false) {
        return labDOM.call('lab_storage_control', field, [value], Number(local), labApplication.configuration);
    }
    try {
        if (!existingEquationControl)
            document.body.appendChild(equationControl);
        for (const value of [undefined, null, '', 'bad', '17.9', '  +53 bits', '1e3', '0x100', '1048577',
                             '\u2003106 trailing', -12, NaN, Infinity, 106]) {
            const parsed = parseInt(String(value), 10);
            const expected = Number.isFinite(parsed) ? Math.min(262144, Math.max(17, parsed)) : 106;
            equal(validPrecisionBits(value, 106), expected, 'decimal-prefix precision conversion and clamp');
        }
        for (const [value, expected] of [[' 5000 trailing', 5000], ['500.9', 500], ['2e4', WorksheetContext.DEFAULT_INTEGRATOR_INTERVAL_CAP],
                                       [undefined, WorksheetContext.DEFAULT_INTEGRATOR_INTERVAL_CAP], [100000, 100000]])
            equal(WorksheetPersistence.statePolicy.intervalCap(value), expected, 'integrator budget whitelist and configured fallback');
        equal(WorksheetPersistence.statePolicy.matrixOperation(' storage-fixture '), 'storage-fixture', 'operation validation uses current options');
        equal(WorksheetPersistence.statePolicy.matrixOperation('not-an-operation'), 'eval', 'unknown operation receives evaluation default');

        const serverFields = labDOM.call<LabWorksheetTestField[]>('lab_storage_fields', 0);
        const localFields = labDOM.call<LabWorksheetTestField[]>('lab_storage_fields', 1);
        equal(serverFields.map(field => field.field).join(','), '0,1,2,3,4', 'server restore order');
        equal(localFields.map(field => field.field).join(','), '1,2,0,3,4', 'local storage read order');
        equal(localFields[0].key, 'mars.exprLab.lastMatrixOperation', 'local storage key compatibility');
        equal(serverFields[3].key, 'integrator_bounds', 'server bounds field compatibility');
        control(0, '  ');
        equal(equationControl.value, DEFAULT_EQUATION_VARIABLE_TEXT, 'blank server variable restores default');
        control(0, '  authored variable  ', true);
        equal(equationControl.value, '  authored variable  ', 'local variable whitespace is retained');
        control(0, '', true);
        equal(equationControl.value, '  authored variable  ', 'empty local variable does not overwrite current control');
        control(1, 'unknown');
        equal(WorksheetContext.matrixOperation.value, 'eval', 'saved invalid operation is normalised');
        control(2, '  α + β  ');
        equal(WorksheetContext.matrixOperand.value, 'α + β', 'server operand trimmed without mathematical rewriting');
        control(2, '  α - β  ', true);
        equal(WorksheetContext.matrixOperand.value, '  α - β  ', 'local operand bytes are retained');
        control(2, null, true);
        equal(WorksheetContext.matrixOperand.value, '  α - β  ', 'missing local operand is not a clearing request');
        control(2, '', true);
        equal(WorksheetContext.matrixOperand.value, '', 'present empty local operand clears the control');
        equal(control(3, '  x = a+b .. a-b  '), 'x = a+b .. a-b', 'server bounds trim only outer whitespace');
        equal(control(3, '  x = a+b .. a-b  ', true), '  x = a+b .. a-b  ', 'local bounds remain opaque');
        equal(control(3, ' '), null, 'empty server bounds request no asynchronous restore');
        control(4, 'bad');
        equal(WorksheetContext.integratorIntervalCap.value, String(WorksheetContext.DEFAULT_INTEGRATOR_INTERVAL_CAP), 'invalid server budget defaults');
        WorksheetContext.integratorIntervalCap.value = '500';
        control(4, '', true);
        equal(WorksheetContext.integratorIntervalCap.value, '500', 'missing local budget preserves current value');

        for (let mode = 0; mode < 5; ++mode) {
            const server = labDOM.call<LabStorageEditor>('lab_storage_editor', mode, ['  opaque μ  '], 0);
            const local = labDOM.call<LabStorageEditor>('lab_storage_editor', mode, ['  opaque μ  '], 1);
            equal(server.text, 'opaque μ', 'server editor outer whitespace is trimmed');
            equal(local.text, '  opaque μ  ', 'local editor whitespace is retained');
            equal(server.flags, native.lab_persist_restore(mode, 1, 0, 0), 'existing native restore flags retained');
            equal(local.flags, native.lab_persist_restore(mode, 1, 0, 1), 'local restore flags retained');
            equal(labDOM.call<LabStorageEditor>('lab_storage_editor', mode, ['partial...'], 0).flags !== 0, mode < 2,
                  'abbreviated non-binding editor copies remain rejected');
        }
        equal(labDOM.call<LabStorageEditor>('lab_storage_editor', 0, [' '], 0).flags, 0, 'empty trimmed server editor is ignored');
        check(labDOM.call<LabStorageEditor>('lab_storage_editor', 0, [' '], 1).flags !== 0, 'local whitespace-only editor remains present');

        labDOM.call('lab_storage_precisions', {precision_bits: {expression: '17', equation: ' 106 bits',
                                                              matrix: '2000000', unknown: '999'}});
        equal(native.lab_workspace_precision(0), 53, 'saved precision respects the binary64 minimum');
        equal(native.lab_workspace_precision(1), 106, 'per-mode precision accepts decimal prefixes');
        equal(native.lab_workspace_precision(3), 262144, 'saved precision clamps at native maximum');
        const unchanged = native.lab_workspace_precision(1);
        labDOM.call('lab_storage_precisions', {precision_bits: '53.9'});
        equal(native.lab_workspace_precision(0), 53, 'legacy scalar precision updates expression');
        equal(native.lab_workspace_precision(1), unchanged, 'legacy scalar precision leaves other modes alone');
        labDOM.call('lab_storage_precisions', {precision_bits: Object.create({equation: '999'})});
        equal(native.lab_workspace_precision(1), unchanged, 'inherited precision fields cannot overwrite native state');
        labDOM.call('lab_storage_precisions', {});
        equal(native.lab_workspace_precision(0), 53, 'absent saved precision leaves state unchanged');

        /** @brief Ask native policy whether the local editor should replace the server copy. */
        function recover(mode: number, server: LabStorageRecord, text: string, stamp: unknown): LabStorageRecovery {
            return labDOM.call<LabStorageRecovery>(
                'lab_storage_recovery', mode, server, [text, stamp], labApplication.configuration);
        }
        equal(recover(0, {expression: 'server', expression_updated_at: 200}, 'local', '100'), null,
              'older local copy cannot replace a populated server copy');
        equal(recover(0, {expression: 'server', expression_updated_at: 200}, ' ', '300'), null,
              'empty local copy cannot replace useful server text');
        const recovery = recover(0, {expression: 'server', expression_updated_at: 200}, '  exact local μ  ', '300');
        equal(recovery.text, 'exact local μ', 'newer recovery preserves opaque trimmed source');
        equal(recovery.updatedAt, 300, 'saved timestamp is numeric');
        const patch = labDOM.call<LabStorageRecord>('lab_storage_recovered', 0, recovery, [recovery.text], 999);
        equal(patch.expression, recovery.text, 'recovery patch owns the selected expression');
        equal(patch.expression_updated_at, 300, 'valid local timestamp wins over clock fallback');
        const missing = recover(1, {equation: WorksheetContext.DEFAULT_EQUATION_TEXT}, 'native equation', '0');
        check(missing, 'local equation can replace an undated server placeholder');
        const equationPatch = labDOM.call<LabStorageRecord>('lab_storage_recovered', 1, missing, ['native canonical equation'], 999);
        equal(equationPatch.equation, 'native canonical equation', 'recovery uses prepared native text');
        equal(equationPatch.equation_updated_at, 999, 'missing local timestamp uses supplied clock');
        equal(recover(4, {}, 'integrator', '300'), null, 'local timestamp recovery is restricted to its two modes');

        const pendingBounds = LabTestBrowser.signal();
        const enteredBounds = LabTestBrowser.signal();
        const readKeys: string[] = [];
        Reflect.set(WorksheetWorkspace.integrator, 'restoreIntegratorBoundsText', async function restoreBounds(text: string) {
            equal(text, 'opaque bounds', 'asynchronous restore receives C-normalised bounds');
            enteredBounds.resolve();
            await pendingBounds.promise;
        });
        WorksheetContext.integratorIntervalCap.value = '500';
        const storedControls: LabStorageRecord = {
            integrator_bounds: ' opaque bounds ',
            integrator_interval_cap: '5000'
        };

        /** @brief Record storage reads so their order across the bounds await remains visible. */
        function readControl(key: string): unknown {
            readKeys.push(key);
            return storedControls[key];
        }

        const pendingControls = WorksheetPersistence.restoration.restoreControls(readControl);
        await enteredBounds.promise;
        equal(WorksheetContext.integratorIntervalCap.value, '500', 'later controls are not projected across a pending bounds request');
        check(!readKeys.includes('integrator_interval_cap'), 'later storage reads also remain after the bounds await');
        pendingBounds.resolve();
        await pendingControls;
        equal(WorksheetContext.integratorIntervalCap.value, '5000', 'budget projection follows completed bounds preparation');

        const preparation = LabTestBrowser.signal();
        let owned = true;
        let updates = 0;

        /** @brief Resolve history ownership when the asynchronous controller checks it. */
        function ownsHistory(): boolean {
            return owned;
        }
        Reflect.set(WorksheetRequests.presentation, 'prepareLabEditor', () => preparation.promise);
        Reflect.set(WorksheetBindings.controller, 'applyUpdatedBindingExpression', () => ++updates);
        WorksheetContext.matrixOperand.value = 'current operand';
        const stale = WorksheetPersistence.navigation.restore({mode: 'matrix', text: 'old', operation: 'eval', operand: 'old operand'},
                                          ownsHistory);
        owned = false;
        preparation.resolve();
        equal(await stale, false, 'history ownership is checked after editor preparation');
        equal(WorksheetContext.matrixOperand.value, 'current operand', 'stale preparation cannot project historical controls');
        equal(updates, 0, 'stale preparation cannot publish historical editor text');

        const calendarWait = LabTestBrowser.signal();
        const calendarEntered = LabTestBrowser.signal();
        let cleared = 0;

        /** @brief Count deferred source and binding resets. */
        function clear(): number {
            return ++cleared;
        }
        Reflect.set(WorksheetBindings.controller, 'clearExpressionSource', clear);
        Reflect.set(WorksheetBindings.panel, 'clearVariableValues', clear);
        Reflect.set(WorksheetLocations.calendarActions, 'restoreCalendarHistory', async function restoreCalendar(
            mode: WorksheetMode, state: WorksheetSnapshot, guard: WorksheetGuard
        ) {
            equal(mode, 'almanac', 'calendar mode is retained');
            equal(state.marker, 'native', 'calendar history retains its native record');
            check(guard(), 'calendar restoration receives ownership guard');
            calendarEntered.resolve();
            await calendarWait.promise;
        });
        owned = true;
        WorksheetContext.expr.value = 'current editor';
        const staleCalendar = WorksheetPersistence.navigation.restore(
            {mode: 'almanac', almanac: {marker: 'native'}} as unknown as WorksheetSnapshot, ownsHistory);
        await calendarEntered.promise;
        owned = false;
        calendarWait.resolve();
        equal(await staleCalendar, false, 'calendar ownership is checked after asynchronous restoration');
        equal(WorksheetContext.expr.value, 'current editor', 'stale calendar completion cannot reset editor text');
        equal(cleared, 0, 'stale calendar completion cannot clear bindings or source');
        Reflect.set(WorksheetLocations.calendarActions, 'restoreCalendarHistory', async () => {});
        equal(await WorksheetPersistence.navigation.restore({mode: 'datetime'} as unknown as WorksheetSnapshot), true, 'current calendar history completes');
        equal(WorksheetContext.expr.value, WorksheetContext.DEFAULT_DATETIME_TEXT, 'calendar completion projects its native configured editor default');
        equal(cleared, 2, 'current calendar completion clears source and bindings once each');
    } finally {
        Reflect.set(WorksheetRequests.presentation, 'prepareLabEditor', saved.prepare);
        Reflect.set(WorksheetWorkspace.integrator, 'restoreIntegratorBoundsText', saved.bounds);
        Reflect.set(WorksheetLocations.calendarActions, 'restoreCalendarHistory', saved.calendar);
        Reflect.set(WorksheetBindings.controller, 'applyUpdatedBindingExpression', saved.updated);
        Reflect.set(WorksheetBindings.controller, 'clearExpressionSource', saved.source);
        Reflect.set(WorksheetBindings.panel, 'clearVariableValues', saved.bindings);
        option.remove();
        for (let index = 0; index < controls.length; ++index) {
            controls[index].value = saved.values[index];
        }
        if (!existingEquationControl)
            equationControl.remove();
        for (let index = 0; index < saved.precision.length; ++index) {
            native.lab_workspace_precision_set(index, saved.precision[index]);
        }
    }
    return true;
};
