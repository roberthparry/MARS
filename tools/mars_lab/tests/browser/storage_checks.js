/** Saved-state policies and guarded restoration against the C/WASM storage controller. */
window.checkLabStorage = async function checkLabStorage() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Storage: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const deferred = () => {
        let resolve;
        const promise = new Promise(accept => resolve = accept);
        return {promise, resolve};
    };
    const native = labWire.exports();
    const existingEquationControl = document.getElementById('equationVariable');
    const equationControl = existingEquationControl || document.createElement('input');
    if (!existingEquationControl) {
        equationControl.id = 'equationVariable';
        equationControl.hidden = true;
    }
    const controls = [equationControl, matrixOperation, matrixOperand, integratorIntervalCap, expr];
    const saved = {
        values: controls.map(node => node.value),
        precision: WORKSPACE_MODE_NAMES.map((_, index) => native.lab_workspace_precision(index)),
        prepare: prepareLabEditor,
        bounds: restoreIntegratorBoundsText,
        calendar: restoreCalendarHistory,
        updated: applyUpdatedBindingExpression,
        source: clearExpressionSource,
        bindings: clearVariableValues
    };
    const option = document.createElement('option');
    option.value = 'storage-fixture';
    matrixOperation.appendChild(option);
    const control = (field, value, local = false) =>
        labDOM.call('lab_storage_control', field, [value], Number(local), labConfig);
    try {
        if (!existingEquationControl)
            document.body.appendChild(equationControl);
        for (const value of [undefined, null, '', 'bad', '17.9', '  +53 bits', '1e3', '0x100', '1048577',
                             '\u2003106 trailing', -12, NaN, Infinity, 106]) {
            const parsed = parseInt(String(value), 10);
            const expected = Number.isFinite(parsed) ? Math.min(1048576, Math.max(17, parsed)) : 106;
            equal(validPrecisionBits(value, 106), expected, 'decimal-prefix precision conversion and clamp');
        }
        for (const [value, expected] of [[' 5000 trailing', 5000], ['500.9', 500], ['2e4', DEFAULT_INTEGRATOR_INTERVAL_CAP],
                                       [undefined, DEFAULT_INTEGRATOR_INTERVAL_CAP], [100000, 100000]])
            equal(validIntegratorIntervalCap(value), expected, 'integrator budget whitelist and configured fallback');
        equal(validMatrixOperation(' storage-fixture '), 'storage-fixture', 'operation validation uses current options');
        equal(validMatrixOperation('not-an-operation'), 'eval', 'unknown operation receives evaluation default');

        const serverFields = labDOM.call('lab_storage_fields', 0);
        const localFields = labDOM.call('lab_storage_fields', 1);
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
        equal(matrixOperation.value, 'eval', 'saved invalid operation is normalised');
        control(2, '  α + β  ');
        equal(matrixOperand.value, 'α + β', 'server operand trimmed without mathematical rewriting');
        control(2, '  α - β  ', true);
        equal(matrixOperand.value, '  α - β  ', 'local operand bytes are retained');
        control(2, null, true);
        equal(matrixOperand.value, '  α - β  ', 'missing local operand is not a clearing request');
        control(2, '', true);
        equal(matrixOperand.value, '', 'present empty local operand clears the control');
        equal(control(3, '  x = a+b .. a-b  '), 'x = a+b .. a-b', 'server bounds trim only outer whitespace');
        equal(control(3, '  x = a+b .. a-b  ', true), '  x = a+b .. a-b  ', 'local bounds remain opaque');
        equal(control(3, ' '), null, 'empty server bounds request no asynchronous restore');
        control(4, 'bad');
        equal(integratorIntervalCap.value, String(DEFAULT_INTEGRATOR_INTERVAL_CAP), 'invalid server budget defaults');
        integratorIntervalCap.value = '500';
        control(4, '', true);
        equal(integratorIntervalCap.value, '500', 'missing local budget preserves current value');

        for (let mode = 0; mode < 5; ++mode) {
            const server = labDOM.call('lab_storage_editor', mode, ['  opaque μ  '], 0);
            const local = labDOM.call('lab_storage_editor', mode, ['  opaque μ  '], 1);
            equal(server.text, 'opaque μ', 'server editor outer whitespace is trimmed');
            equal(local.text, '  opaque μ  ', 'local editor whitespace is retained');
            equal(server.flags, native.lab_persist_restore(mode, 1, 0, 0), 'existing native restore flags retained');
            equal(local.flags, native.lab_persist_restore(mode, 1, 0, 1), 'local restore flags retained');
            equal(labDOM.call('lab_storage_editor', mode, ['partial...'], 0).flags !== 0, mode < 2,
                  'abbreviated non-binding editor copies remain rejected');
        }
        equal(labDOM.call('lab_storage_editor', 0, [' '], 0).flags, 0, 'empty trimmed server editor is ignored');
        check(labDOM.call('lab_storage_editor', 0, [' '], 1).flags !== 0, 'local whitespace-only editor remains present');

        labDOM.call('lab_storage_precisions', {precision_bits: {expression: '17', equation: ' 106 bits',
                                                              matrix: '2000000', unknown: '999'}});
        equal(native.lab_workspace_precision(0), 17, 'saved precision retains legacy seventeen-bit minimum');
        equal(native.lab_workspace_precision(1), 106, 'per-mode precision accepts decimal prefixes');
        equal(native.lab_workspace_precision(3), 1048576, 'saved precision clamps at native maximum');
        const unchanged = native.lab_workspace_precision(1);
        labDOM.call('lab_storage_precisions', {precision_bits: '53.9'});
        equal(native.lab_workspace_precision(0), 53, 'legacy scalar precision updates expression');
        equal(native.lab_workspace_precision(1), unchanged, 'legacy scalar precision leaves other modes alone');
        labDOM.call('lab_storage_precisions', {precision_bits: Object.create({equation: '999'})});
        equal(native.lab_workspace_precision(1), unchanged, 'inherited precision fields cannot overwrite native state');
        labDOM.call('lab_storage_precisions', {});
        equal(native.lab_workspace_precision(0), 53, 'absent saved precision leaves state unchanged');

        const recover = (mode, server, text, stamp) =>
            labDOM.call('lab_storage_recovery', mode, server, [text, stamp], labConfig);
        equal(recover(0, {expression: 'server', expression_updated_at: 200}, 'local', '100'), null,
              'older local copy cannot replace a populated server copy');
        equal(recover(0, {expression: 'server', expression_updated_at: 200}, ' ', '300'), null,
              'empty local copy cannot replace useful server text');
        const recovery = recover(0, {expression: 'server', expression_updated_at: 200}, '  exact local μ  ', '300');
        equal(recovery.text, 'exact local μ', 'newer recovery preserves opaque trimmed source');
        equal(recovery.updatedAt, 300, 'saved timestamp is numeric');
        const patch = labDOM.call('lab_storage_recovered', 0, recovery, [recovery.text], 999);
        equal(patch.expression, recovery.text, 'recovery patch owns the selected expression');
        equal(patch.expression_updated_at, 300, 'valid local timestamp wins over clock fallback');
        const missing = recover(1, {equation: DEFAULT_EQUATION_TEXT}, 'native equation', '0');
        check(missing, 'local equation can replace an undated server placeholder');
        const equationPatch = labDOM.call('lab_storage_recovered', 1, missing, ['native canonical equation'], 999);
        equal(equationPatch.equation, 'native canonical equation', 'recovery uses prepared native text');
        equal(equationPatch.equation_updated_at, 999, 'missing local timestamp uses supplied clock');
        equal(recover(4, {}, 'integrator', '300'), null, 'local timestamp recovery is restricted to its two modes');

        const pendingBounds = deferred(), enteredBounds = deferred(), readKeys = [];
        restoreIntegratorBoundsText = async text => {
            equal(text, 'opaque bounds', 'asynchronous restore receives C-normalised bounds');
            enteredBounds.resolve();
            await pendingBounds.promise;
        };
        integratorIntervalCap.value = '500';
        const pendingControls = restoreWorksheetControls(key => {
            readKeys.push(key);
            return {integrator_bounds: ' opaque bounds ', integrator_interval_cap: '5000'}[key];
        });
        await enteredBounds.promise;
        equal(integratorIntervalCap.value, '500', 'later controls are not projected across a pending bounds request');
        check(!readKeys.includes('integrator_interval_cap'), 'later storage reads also remain after the bounds await');
        pendingBounds.resolve();
        await pendingControls;
        equal(integratorIntervalCap.value, '5000', 'budget projection follows completed bounds preparation');

        const preparation = deferred();
        let owned = true, updates = 0;
        prepareLabEditor = () => preparation.promise;
        applyUpdatedBindingExpression = () => ++updates;
        matrixOperand.value = 'current operand';
        const stale = restoreHistoryState({mode: 'matrix', text: 'old', operation: 'eval', operand: 'old operand'},
                                          () => owned);
        owned = false;
        preparation.resolve();
        equal(await stale, false, 'history ownership is checked after editor preparation');
        equal(matrixOperand.value, 'current operand', 'stale preparation cannot project historical controls');
        equal(updates, 0, 'stale preparation cannot publish historical editor text');

        const calendarWait = deferred(), calendarEntered = deferred();
        let cleared = 0;
        clearExpressionSource = clearVariableValues = () => ++cleared;
        restoreCalendarHistory = async (mode, state, guard) => {
            equal(mode, 'almanac', 'calendar mode is retained');
            equal(state.marker, 'native', 'calendar history retains its native record');
            check(guard(), 'calendar restoration receives ownership guard');
            calendarEntered.resolve();
            await calendarWait.promise;
        };
        owned = true;
        expr.value = 'current editor';
        const staleCalendar = restoreHistoryState({mode: 'almanac', almanac: {marker: 'native'}}, () => owned);
        await calendarEntered.promise;
        owned = false;
        calendarWait.resolve();
        equal(await staleCalendar, false, 'calendar ownership is checked after asynchronous restoration');
        equal(expr.value, 'current editor', 'stale calendar completion cannot reset editor text');
        equal(cleared, 0, 'stale calendar completion cannot clear bindings or source');
        restoreCalendarHistory = async () => {};
        equal(await restoreHistoryState({mode: 'datetime'}), true, 'current calendar history completes');
        equal(expr.value, DEFAULT_DATETIME_TEXT, 'calendar completion projects its native configured editor default');
        equal(cleared, 2, 'current calendar completion clears source and bindings once each');
    } finally {
        prepareLabEditor = saved.prepare;
        restoreIntegratorBoundsText = saved.bounds;
        restoreCalendarHistory = saved.calendar;
        applyUpdatedBindingExpression = saved.updated;
        clearExpressionSource = saved.source;
        clearVariableValues = saved.bindings;
        option.remove();
        controls.forEach((node, index) => node.value = saved.values[index]);
        if (!existingEquationControl)
            equationControl.remove();
        saved.precision.forEach((bits, index) => native.lab_workspace_precision_set(index, bits));
    }
    return true;
};
