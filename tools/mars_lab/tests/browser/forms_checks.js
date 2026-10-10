/** Structured C/WASM form regressions; the parent invokes this after labReady. */
window.checkLabForms = async function checkLabForms() {
    const forms = labWire.exports();
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`C forms: ${message}`);
    };
    const equal = (actual, expected, message) =>
        check(Object.is(actual, expected), `${message}: ${actual} != ${expected}`);

    const previousPickerDate = forms.lab_forms_picker_date();
    try {
        forms.lab_forms_picker_close();
        equal(forms.lab_forms_picker_date(), 0, 'closed picker has no visible month');
        equal(forms.lab_forms_picker_set(2024, 2, 29, 2026), 0, 'set cannot reopen a closed picker');
        equal(forms.lab_forms_picker_shift(1, false, 31), 0, 'navigation cannot reopen a closed picker');
        equal(forms.lab_forms_picker_open(2024, 1, 2026), 20240101, 'open owns the visible month');
        equal(forms.lab_forms_picker_shift(1, false, 31), 20240229, 'January day clamps into leap February');
        equal(forms.lab_forms_picker_date(), 20240201, 'navigation stores the new month');
        equal(forms.lab_forms_picker_shift(1, true, 29), 20250228, 'year navigation clamps leap day');
        for (const delta of [NaN, Infinity, 0.5, 120001]) {
            equal(forms.lab_forms_picker_shift(delta, false, 28), 0, 'invalid month shift rejected');
            equal(forms.lab_forms_picker_shift(delta, true, 28), 0, 'invalid year shift rejected');
            equal(forms.lab_forms_picker_date(), 20250201, 'invalid shift preserves native state');
        }
        equal(forms.lab_forms_picker_set(NaN, NaN, NaN, 2026), 20260101, 'input fallbacks owned by C');
        equal(forms.lab_forms_picker_set(10000, 14, 40, 2026), 99991231, 'upper bounds clamp');
        equal(forms.lab_forms_picker_shift(1, false, 31), 99991231, 'upper month navigation saturates');
        equal(forms.lab_forms_picker_shift(100, true, 31), 99991231, 'upper year navigation saturates');
        equal(forms.lab_forms_picker_set(0, 0, 0, 2026), 10101, 'lower bounds clamp');
        equal(forms.lab_forms_picker_shift(-1, false, 1), 10101, 'lower month navigation saturates');
        equal(forms.lab_forms_picker_shift(-100, true, 1), 10101, 'lower year navigation saturates');
    } finally {
        forms.lab_forms_picker_close();
        if (previousPickerDate)
            forms.lab_forms_picker_open(
                forms.lab_forms_date_year(previousPickerDate), forms.lab_forms_date_month(previousPickerDate), 1);
    }

    for (const [year, days] of [[1, 28], [4, 29], [100, 28], [400, 29], [1900, 28], [2000, 29], [2024, 29], [2100, 28]])
        equal(forms.lab_forms_days_in_month(year, 2), days, `February ${year}`);
    for (const args
             of [[NaN, 1, 1], [Infinity, 1, 1], [0, 1, 1], [10000, 1, 1], [2024, 0, 1], [2024, 13, 1], [2024, 2.5, 1],
                 [2024, 2, 30], [1900, 2, 29], [2024, 1, 0]])
        equal(forms.lab_forms_date_valid(...args), 0, `invalid structured date ${args}`);
    equal(forms.lab_forms_date_valid(1, 1, 1), 1, 'earliest date');
    equal(forms.lab_forms_date_valid(9999, 12, 31), 1, 'latest date');
    equal(forms.lab_forms_month_weekday(1, 1), 0, '0001 starts on Monday');
    equal(forms.lab_forms_month_weekday(2000, 1), 5, '2000 starts on Saturday');
    equal(forms.lab_forms_month_weekday(2024, 2), 3, 'February 2024 starts on Thursday');
    equal(forms.lab_forms_month_weekday(NaN, 2), -1, 'invalid weekday');
    equal(forms.lab_forms_calendar_cell(2024, 2, 0), 20240129, 'leading date cell');
    equal(forms.lab_forms_calendar_cell(2024, 2, 31), 20240229, 'leap date cell');
    equal(forms.lab_forms_calendar_cell(2024, 2, 41), 20240310, 'trailing date cell');
    equal(forms.lab_forms_calendar_cell(9999, 12, 41), 0, 'out-of-range grid cell disabled');
    equal(forms.lab_forms_calendar_cell(2024, 2, 42), 0, 'grid slot bound');
    equal(forms.lab_forms_calendar_cell(2024, 2, NaN), 0, 'invalid grid slot');
    const grid = (...args) => {
        const pointer = forms.lab_forms_calendar_grid(...args);
        check(pointer !== 0, 'valid grid buffer');
        return new Uint32Array(forms.memory.buffer, pointer, 168).slice();
    };
    const leapGrid = grid(2024, 2, 2024, 2, 29, 2024, 2, 29);
    equal(leapGrid.slice(0, 4).join(','), '2024,1,29,3', 'leading outside-month cell');
    equal(leapGrid.slice(124, 128).join(','), '2024,2,29,13', 'today and selected flags coexist');
    equal(leapGrid.slice(164).join(','), '2024,3,10,3', 'trailing outside-month cell');
    const outsideGrid = grid(2024, 2, 2024, 1, 29, NaN, 1, 1);
    equal(outsideGrid[3], 11, 'outside-month date can be selected');
    equal(leapGrid[127], 13, 'copied grid survives another export call');
    for (const [year, month] of [[1, 1], [1900, 2], [2000, 2], [2024, 12], [9999, 12]]) {
        const cells = grid(year, month, 0, 0, 0, 0, 0, 0);
        for (let slot = 0; slot < 42; ++slot) {
            const [y, m, d, flags] = cells.subarray(slot * 4, slot * 4 + 4);
            const packed = forms.lab_forms_calendar_cell(year, month, slot);
            equal(y * 10000 + m * 100 + d, packed, 'batched and scalar date agreement');
            equal(flags, packed ? (m === month ? 1 : 3) : 2, 'boundary and absent-selection flags');
        }
    }
    const retainedPointer = forms.lab_forms_calendar_grid(2024, 2, 0, 0, 0, 0, 0, 0);
    const retained = new Uint32Array(forms.memory.buffer, retainedPointer, 168).slice().join(',');
    for (const [year, month] of [[NaN, 1], [2024, Infinity], [0, 1], [2024, 13]]) {
        equal(forms.lab_forms_calendar_grid(year, month, 0, 0, 0, 0, 0, 0), 0, 'invalid grid rejected');
        equal(
            new Uint32Array(forms.memory.buffer, retainedPointer, 168).join(','), retained,
            'invalid request preserves borrowed grid');
    }
    equal(forms.lab_forms_shift_month(2024, 1, -1), 20231201, 'backward year rollover');
    equal(forms.lab_forms_shift_month(2024, 12, 1), 20250101, 'forward year rollover');
    equal(forms.lab_forms_shift_month(1, 1, -1), 10101, 'minimum month saturation');
    equal(forms.lab_forms_shift_month(9999, 12, 1), 99991201, 'maximum month saturation');
    equal(forms.lab_forms_shift_month(2024, 1, 1e300), 0, 'oversized shift rejected before integer conversion');
    equal(forms.lab_forms_shift_year(9999, 100), 9999, 'year shift saturates');
    equal(forms.lab_forms_shift_year(1, -100), 1, 'backward year shift saturates');
    equal(forms.lab_forms_shift_year(2024, 0.5), 0, 'fractional year shift rejected');
    equal(forms.lab_forms_clamp_day(2023, 2, 31), 28, 'day clamps to month');
    equal(forms.lab_forms_clamp_day(2024, 2, 31), 29, 'day clamps to leap month');
    equal(forms.lab_forms_clamp(1e300, 1, 9999, 2024), 9999, 'huge finite year clamps');
    equal(forms.lab_forms_clamp(NaN, 1, 9999, 2024), 2024, 'invalid year falls back');
    equal(forms.lab_forms_year_step(1, 1), 100, 'control takes priority over shift');
    equal(forms.lab_forms_year_step(0, 1), 10, 'shift year step');
    equal(forms.lab_forms_year_step(0, 0), 1, 'ordinary year step');

    equal(forms.lab_forms_utc_milliseconds(1970, 1, 1, 0, 0, 0), 0, 'Unix epoch');
    equal(forms.lab_forms_utc_milliseconds(1, 1, 1, 0, 0, 0), -62135596800000, 'early years are not mapped to 1900');
    equal(forms.lab_forms_offset_hours(20700000, 0), 5.75, 'quarter-hour positive offset');
    equal(forms.lab_forms_offset_hours(-12600000, 0), -3.5, 'negative fractional offset');
    check(Number.isNaN(forms.lab_forms_offset_hours(Infinity, 0)), 'non-finite offset rejected');
    check(Number.isNaN(forms.lab_forms_utc_milliseconds(2024, 2, 30, 0, 0, 0)), 'invalid civil time rejected');
    equal(forms.lab_forms_round_offset(-3.5), -3.5, 'negative half-hour display');
    equal(forms.lab_forms_nearly_equal(0, 0.0000005, 0.000001), 1, 'coordinate tolerance');
    equal(forms.lab_forms_nearly_equal(0, 0.001, 0.000001), 0, 'coordinate mismatch');
    equal(forms.lab_forms_nearly_equal(Infinity, Infinity, 0.000001), 0, 'non-finite coordinates');
    equal(forms.lab_forms_nearly_equal(1, 1, -1), 0, 'negative tolerance');

    const removedIntegrator = await labFetch('/js/integrator.js');
    check(
        removedIntegrator.status === 404 && !labDefinitionScripts.includes('integrator'), 'obsolete integrator script');
    for (const free of [false, true]) {
        for (const lo of ['', 'a + μ']) {
            for (const hi of ['', 'a - μ']) {
                const expected = free ? 'free [α]' : hi ? lo ? `[α] = ${lo} .. ${hi}` : `[α] = ${hi}` : '[α]';
                equal(
                    integratorRowText({kind: free ? 'free' : 'bound', name: '[α]', lo, hi}), expected,
                    'native row formatting preserves structured authored fields');
            }
        }
    }
    const batchText = rows => labDOM.call('lab_binding_rows_text', rows);
    equal(batchText(null), '', 'absent row array is empty');
    equal(batchText(new Array(2)), '', 'sparse array holes do not manufacture rows');
    equal(batchText([{name: 'x'}, {name: []}]), 'x', 'empty formatted rows do not add separators');
    equal(batchText([, null, , {name: []}, {name: 'y'}]), 'x\ny', 'holes and empty rows preserve separator placement');
    equal(batchText({name: 'ignored'}), '', 'non-array is not treated as a row list');
    equal(batchText([null, {name: 0, lo: false, hi: 2}]), 'x\nx = 2', 'row defaults and coercion match the editor');
    const opaqueName = '\uFEFF[α]\0😀';
    equal(
        batchText([{name: opaqueName, lo: ' a + μ ', hi: ' a - μ '}]), opaqueName + ' =  a + μ  ..  a - μ ',
        'BOM, NUL, supplementary Unicode and authored whitespace preserved');
    const rows = Array.from({length: 256}, (_, index) => ({name: 'x' + index, kind: index % 2 ? 'free' : 'bound'}));
    equal(
        batchText(rows), rows.map(row => row.kind === 'free' ? 'free ' + row.name : row.name).join('\n'),
        'complete 256-row batch uses bounded temporary handles');
    equal(batchText([...rows, {name: 'overflow'}]), null, 'oversized batch rejected');
    for (const field of ['name', 'lo', 'hi']) {
        for (const invalid of ['\uD800', '\uDC00']) {
            equal(
                batchText([{name: 'x'}, {name: 'y', [field]: invalid}]), null,
                'invalid Unicode rejects the entire batch, including unused bounds');
        }
    }
    const capacity = forms.lab_workspace_capacity();
    const fullName = 'x'.repeat(capacity);
    equal(batchText([{name: fullName}]).length, capacity, 'exact output capacity accepted');
    equal(batchText([{name: fullName, kind: 'free'}]), null, 'notation included in total output limit');
    equal(batchText([{name: fullName}, {name: 'y'}]), null, 'batch separator and following row cannot overflow');
    equal(
        batchText([{name: 'x'.repeat(capacity - 2)}, {name: 'y'}]).length, capacity,
        'last row and separator fit the exact batch limit');
    equal(batchText([{name: fullName, lo: '1'}]), null, 'unused fields still obey combined input capacity');
    equal(batchText([{name: 'μ'.repeat(capacity / 2 + 1)}]), null, 'capacity counts UTF-8 bytes');
    equal(batchText([{name: 'ok', hi: '1'}]), 'ok = 1', 'rejected batch does not poison subsequent calls');
    const revision = forms.lab_rows_revision_next();
    check(revision && forms.lab_rows_revision_accept(revision, true), 'current form revision accepted');
    check(!forms.lab_rows_revision_accept(revision, false), 'changed expression rejects response');
    check(!forms.lab_rows_revision_accept(0, true), 'unissued form revision rejected');
    const newerRevision = forms.lab_rows_revision_next();
    check(
        newerRevision !== revision && !forms.lab_rows_revision_accept(revision, true) &&
            forms.lab_rows_revision_accept(newerRevision, true),
        'newer form work supersedes previous preparation');
    const output = new Uint8Array(forms.memory.buffer, forms.lab_workspace_input(1), 8);
    output.fill(77);
    for (const lengths of [[-1, 0, 0], [0, -1, 0], [0, 0, -1], [4194304, 1, 0]]) {
        equal(forms.lab_rows_text(...lengths, false), -1, 'invalid structured row lengths');
        check(output.every(value => value === 77), 'invalid row leaves output unchanged');
    }
    equal(forms.lab_rows_text(4194304, 0, 0, true), -1, 'notation cannot overflow output');
    check(output.every(value => value === 77), 'oversized row leaves output unchanged');
    for (const offset of [-1, capacity, capacity + 1]) {
        equal(forms.lab_rows_text_append(1, 0, 0, false, offset), -1, 'invalid append extent rejected');
        check(output.every(value => value === 77), 'invalid append preserves preceding output');
    }
    equal(forms.lab_forms_row_removable(0, 2, 1), 0, 'last integration variable protected');
    equal(forms.lab_forms_row_removable(1, 2, 1), 1, 'free parameter removable');
    equal(forms.lab_forms_row_removable(1, 1, 0), 0, 'last row protected');
    const stageRows = flags => new Uint32Array(forms.memory.buffer, forms.lab_rows_input(), flags.length).set(flags);
    const selectRows = flags => {
        stageRows(flags);
        const pointer = forms.lab_rows_select(flags.length);
        check(pointer !== 0, 'valid active-row plan');
        const [bounds, active] = new Uint32Array(forms.memory.buffer, pointer, 2);
        return Array.from(new Uint32Array(forms.memory.buffer, pointer, 2 + bounds + active));
    };
    const mergeRows = (flags, bounds) => {
        stageRows(flags);
        const pointer = forms.lab_rows_merge(flags.length, bounds);
        check(pointer !== 0, 'valid reconciled-row plan');
        const count = new Uint32Array(forms.memory.buffer, pointer, 1)[0];
        return Array.from(new Uint32Array(forms.memory.buffer, pointer + 4, count));
    };
    const editRows = (flags, index, operation) => {
        stageRows(flags);
        const pointer = forms.lab_rows_edit(flags.length, index, operation);
        if (!pointer)
            return null;
        const count = new Uint32Array(forms.memory.buffer, pointer, 1)[0];
        return Array.from(new Uint32Array(forms.memory.buffer, pointer + 4, count * 2));
    };
    for (let length = 1; length <= 3; ++length) {
        for (let encoded = 0; encoded < 16 ** length; ++encoded) {
            const flags = Array.from({length}, (_, i) => (encoded >>> (4 * i)) & 15);
            for (let index = 0; index < length; ++index) {
                for (let operation = 1; operation <= 3; ++operation) {
                    const expected = flags.map((flag, i) => [i, flag]);
                    const bounds = flags.filter(flag => !(flag & 1)).length;
                    if (operation === 3 && (length === 1 || (bounds === 1 && !(flags[index] & 1)))) {
                        equal(editRows(flags, index, operation), null, 'remove protects sole row and last bound');
                        continue;
                    }
                    if (operation === 1) {
                        expected[index][1] ^= 1;
                        if (expected[index][1] & 1)
                            expected[index][1] &= ~6;
                    } else if (operation === 2) {
                        expected.splice(index + 1, 0, [256, 0]);
                    } else {
                        expected.splice(index, 1);
                    }
                    if (!expected.some(([, flag]) => !(flag & 1)))
                        expected.push([256, 0]);
                    equal(
                        editRows(flags, index, operation).join(','), expected.flat().join(','), 'native row edit plan');
                }
            }
        }
    }
    equal(editRows(Array(255).fill(1), 0, 2).length, 512, 'insertion reaches row limit exactly');
    const previousEdit = forms.lab_rows_edit(255, 0, 2);
    const previousWords = Array.from(new Uint32Array(forms.memory.buffer, previousEdit, 513));
    for (const [flags, index, operation] of [
             [Array(256).fill(1), 0, 2], [[0, ...Array(255).fill(1)], 0, 1], [[16], 0, 1], [[0], 1, 1], [[0], 0, 4],
             [[], 0, 1]]) {
        equal(editRows(flags, index, operation), null, 'invalid or overflowing edit rejected');
        equal(
            Array.from(new Uint32Array(forms.memory.buffer, previousEdit, 513)).join(','), previousWords.join(','),
            'rejected edit preserves previous output');
    }
    const authoredRows =
        [{kind: 'bound', name: 'x', lo: 'sin(μ)', hi: 'a + b'}, {kind: 'free', name: 'μ', lo: '', hi: ''}];
    const toggledRows = planIntegratorRowEdit(authoredRows, 0, 1);
    equal(toggledRows.length, 3, 'last bound toggle appends a replacement');
    equal(toggledRows[0].lo, '', 'toggle to free clears lower bound');
    equal(toggledRows[0].hi, '', 'toggle to free clears upper bound');
    equal(authoredRows[0].lo, 'sin(μ)', 'edit does not mutate authored source rows');
    equal(planIntegratorRowEdit(authoredRows, 0, 2)[0].hi, 'a + b', 'insertion retains exact bound text');
    check(!labDefinitionScripts.includes('mobile'), 'retired mobile adapter still loaded');

    // Exhaust every flag combination for up to three rows against the prior selection semantics.
    for (let length = 0; length <= 3; ++length) {
        for (let packed = 0; packed < 16 ** length; ++packed) {
            const flags = Array.from({length}, (_, index) => (packed >>> (4 * index)) & 15);
            const boundCount = flags.filter(flag => !(flag & 1)).length;
            const bounds = [], active = [];
            flags.forEach((flag, index) => {
                if (flag & 1) {
                    if (flag & 8)
                        active.push(index);
                } else if (boundCount === 1 || (flag & 14)) {
                    bounds.push(index);
                    active.push(index);
                }
            });
            if (!bounds.length)
                bounds.push(256);
            if (!active.length)
                active.push(256);
            equal(
                selectRows(flags).join(','), [bounds.length, active.length, ...bounds, ...active].join(','),
                'batched selection preserves bound and free-row policy');
            for (let returned = 0; returned <= 4; ++returned) {
                const expected = [];
                let next = 0;
                flags.forEach((flag, index) => {
                    if (!returned || ((flag & 1) && (flag & 8)))
                        expected.push(index);
                    else if (!(flag & 1) && next < returned)
                        expected.push(256 + next++);
                });
                while (next < returned) expected.push(256 + next++);
                equal(
                    mergeRows(flags, returned).join(','), expected.join(','), 'batched reconciliation preserves order');
            }
        }
    }
    equal(selectRows(Array(256).fill(8))[0], 256, 'maximum bound selection');
    equal(mergeRows([], 256).length, 256, 'maximum returned bounds');
    stageRows([9]);
    const planPointer = forms.lab_rows_merge(1, 1);
    const savedPlan = new Uint32Array(forms.memory.buffer, planPointer, 3).slice().join(',');
    for (const [count, bounds] of [[257, 1], [1, 257], [1, 256]]) {
        equal(forms.lab_rows_merge(count, bounds), 0, 'oversized merge rejected');
        equal(
            new Uint32Array(forms.memory.buffer, planPointer, 3).join(','), savedPlan,
            'failed merge preserves previous plan');
    }
    equal(forms.lab_rows_select(257), 0, 'oversized selection rejected');
    stageRows([16]);
    equal(forms.lab_rows_select(1), 0, 'unknown selection flags rejected');
    equal(forms.lab_rows_merge(1, 1), 0, 'unknown merge flags rejected');
    equal(
        new Uint32Array(forms.memory.buffer, planPointer, 3).join(','), savedPlan,
        'invalid flags preserve previous plan');
    equal(forms.lab_forms_name_candidate(0, 0, 0, 0), 0, 'first preferred name');
    equal(forms.lab_forms_name_candidate(511, 0, 0, 0), 9, 'numbered name fallback');
    equal(forms.lab_forms_name_candidate(0xffffffff, 0xffffffff, 0xffffffff, 2047), 107, 'last candidate');
    equal(forms.lab_forms_name_candidate(0xffffffff, 0xffffffff, 0xffffffff, 4095), 0, 'exhausted-name compatibility');

    // Exercise real browser conversion and JS-to-WASM adapters as well as direct exports.
    equal(parseMarsIsoDate('0001-01-01')?.year, 1, 'date input supports early years');
    equal(parseMarsIsoDate('2024-02-29')?.day, 29, 'date input accepts leap date');
    equal(parseMarsIsoDate('2023-02-29'), null, 'date input rejects impossible date');
    equal(parseMarsIsoDate('10000-01-01'), null, 'date input constrained by C range');
    equal(validDateText('2023-02-29', 'fallback'), 'fallback', 'form invalid date fallback');
    equal(forms.lab_forms_clamp(marsFormNumber('13'), 1, 12, 1), 12, 'month input passes through C clamp');
    equal(forms.lab_forms_clamp(marsFormNumber('junk'), 1, 12, 1), 1, 'malformed numeric input falls back');
    equal(numbersNearlyEqual('', '0'), false, 'blank coordinate does not match zero');
    equal(numbersNearlyEqual('51.5', '51.5000001'), true, 'coordinate input adapter');
    equal(formatOffsetHours(-3.5), '-3.5', 'offset formatting adapter');
    equal(timeZoneOffsetHours('UTC', '2024-02-29'), 0, 'browser timezone with C arithmetic');
    equal(timeZoneOffsetHours('Asia/Kathmandu', '2024-02-29'), 5.75, 'fractional timezone with C arithmetic');
    equal(timeZoneOffsetHours('Invalid/Zone', '2024-02-29'), null, 'unsupported timezone fallback');
    equal(integratorDefaultVariableName([{name: 'x'}, {name: ' y '}, {name: 'x'}]), 'z', 'name occupancy adapter');
    equal(
        integratorDefaultVariableName(integratorCandidateNames.map(name => ({name}))), 'x', 'name exhaustion adapter');
    equal(
        integratorRowText({kind: 'bound', name: 'x', lo: 'a+b', hi: 'a-b'}), 'x = a+b .. a-b',
        'row policy preserves authored symbolic bounds');
    equal(integratorRowText({kind: 'free', name: '[α]', lo: '0', hi: '1'}), 'free [α]', 'free row adapter');

    const nativeRows = await parseIntegratorBoundsText('x = f(a,b) .. a-b\nfree [α]');
    equal(nativeRows[0].lo, 'f(a,b)', 'native nested bound parsing');
    equal(nativeRows[1].name, '[α]', 'native authored parameter identity');
    equal(await formatAlmanacTimeInput('123456,789'), '12:34:56.789', 'native clock formatting');
    equal((await townValueParts('Name||-3|5')).longitude, '-3', 'native town empty-field positions');

    const savedRows = currentIntegratorRows();
    const savedReferences = integratorReferenceMetadata;
    const savedDate = datetimeDate.value;
    const savedIntervals = integratorIntervalCap.value;
    const savedPicker = {...marsDatePickerState};
    const wasHidden = marsDatePicker.classList.contains('hidden');
    const savedRequest = requestLabForms;
    try {
        const rowInputs = [
            {kind: 'bound', name: 'x', lo: '', hi: '', referenced: false},
            {kind: 'free', name: 'a', lo: '', hi: '', referenced: true},
            {kind: 'bound', name: 'y', lo: 'f(a,b)', hi: 'a-b', referenced: false}
        ];
        installIntegratorReferenceMetadata('test-row-plan', {rows: rowInputs, references_valid: true});
        const selection = activeIntegratorRowPlan(rowInputs, 'test-row-plan');
        equal(selection.bounds.length, 1, 'adapter selects explicit bound');
        equal(selection.bounds[0], rowInputs[2], 'adapter preserves bound object and authored text');
        equal(selection.rows.map(row => row.name).join(','), 'a,y', 'adapter retains free parameter ordering');
        equal(
            activeIntegratorRowPlan(rowInputs, 'stale-metadata').rows.length, 3,
            'stale references conservatively retain all rows');
        renderIntegratorRows(rowInputs);
        applyIntegratorResultBound({
            bounds: [{kind: 'bound', name: 'z', lo: 'a+b', hi: 'a-b'}],
            binding_values: [{name: 'a', kind: 'variable'}]
        });
        equal(currentIntegratorRows().map(row => row.name).join(','), 'z,a', 'DOM applies C reconciliation order');
        equal(currentIntegratorRows()[0].hi, 'a-b', 'DOM reconciliation preserves symbolic bounds');
        const beforeEmptyResult = integratorBoundStack.innerHTML;
        applyIntegratorResultBound({bounds: []});
        equal(integratorBoundStack.innerHTML, beforeEmptyResult, 'empty result leaves DOM unchanged');
        integratorReferenceMetadata = savedReferences;
        let detailCalls = 0;
        requestLabForms = async () => {
            ++detailCalls;
            throw new Error('Catalogue display must not require form preparation');
        };
        const catalogueSelect = document.createElement('select');
        const catalogueTowns = await populateTownSelect(catalogueSelect, 'GB-WLS');
        check(catalogueTowns.length > 0, 'native catalogue contains Welsh towns');
        equal(detailCalls, 0, 'town population makes no formatting request');
        equal(catalogueSelect.options[0].value, catalogueTowns[0].value, 'native town key applied verbatim');
        equal(
            catalogueSelect.options[0].dataset.detail, catalogueTowns[0].detail, 'native town detail applied verbatim');
        check(Boolean(catalogueTowns[0].detail), 'native catalogue supplies coordinate detail');
        requestLabForms = savedRequest;
        integratorIntervalCap.value = '500';
        equal(requestedIntegratorIntervalCap(), 500, 'supported interval budget');
        integratorIntervalCap.value = 'unsupported';
        equal(
            requestedIntegratorIntervalCap(), DEFAULT_INTEGRATOR_INTERVAL_CAP, 'unsupported interval budget fallback');
        renderIntegratorRows([{kind: 'bound', name: 'x', lo: '', hi: ''}, {kind: 'free', name: 'y', lo: '', hi: ''}]);
        const removeButtons = integratorBoundStack.querySelectorAll('.integrator-bound-remove');
        check(removeButtons[0].disabled && !removeButtons[1].disabled, 'DOM removal controls obey C policy');
        check(
            integratorBoundStack.querySelectorAll('.integrator-bound-row')[1]
                .querySelector('[data-integrator-lower]')
                .disabled,
            'free-row bounds remain disabled');
        // Resolve controlled requests explicitly: no sleeps or live network timing assumptions.
        let finishPreparation;
        requestLabForms = () => new Promise(resolve => {
            finishPreparation = resolve;
        });
        const preparation = refreshIntegratorForms();
        const nameInput = integratorBoundStack.querySelector('[data-integrator-name]');
        nameInput.value = 'newer';
        nameInput.dispatchEvent(new Event('input', {bubbles: true}));
        finishPreparation(
            {rows: [{kind: 'bound', name: 'old', lo: '', hi: '', referenced: true}], references_valid: true});
        equal(await preparation, null, 'pending preparation invalidated by typing');
        equal(nameInput.value, 'newer', 'delayed response preserves latest typing');
        const select = document.createElement('select');
        const restoration = restoreTownSelection(select, '', 'Old|1|2|', '', '');
        select.dispatchEvent(new Event('change'));
        equal(await restoration, null, 'town selection event invalidates pending restoration');

        const originalRows = integratorBoundStack.innerHTML;
        const originalTown = select.innerHTML;
        equal(
            await restoreIntegratorBoundsText('z = 0 .. 9', () => false), false, 'initially stale integrator callback');
        equal(
            await restoreTownSelection(select, '', 'Old|1|2|', '', '', () => false), null,
            'initially stale town callback');
        equal(integratorBoundStack.innerHTML, originalRows, 'false callback leaves integrator DOM unchanged');
        equal(select.innerHTML, originalTown, 'false callback leaves town DOM unchanged');

        let current = true;
        const guardedBounds = restoreIntegratorBoundsText('z = 0 .. 9', () => current);
        current = false;
        finishPreparation({rows: [{kind: 'bound', name: 'z', lo: '0', hi: '9'}], references_valid: true});
        equal(await guardedBounds, false, 'integrator callback checked after await');
        equal(integratorBoundStack.innerHTML, originalRows, 'stale awaited bounds leave DOM unchanged');

        current = true;
        const guardedTown = restoreTownSelection(select, '', 'Old|1|2|', '', '', () => current);
        const populatedTown = select.innerHTML;
        const populatedValue = select.value;
        current = false;
        equal(await guardedTown, null, 'town callback checked after await');
        equal(select.innerHTML, populatedTown, 'stale town preparation cannot write option details');
        equal(select.value, populatedValue, 'stale town restoration cannot change selection');
        requestLabForms = savedRequest;
        datetimeDate.value = '2024-02-29';
        const button = document.querySelector('[data-date-target="datetimeDate"]');
        openMarsDatePicker(datetimeDate, button);
        equal(marsDatePickerGrid.children.length, 42, 'DOM calendar has 42 cells');
        equal(
            marsDatePickerGrid.querySelector('.selected')?.dataset.isoDate, '2024-02-29',
            'selected cell survives C migration');
        equal(marsDatePickerGrid.children[0].dataset.isoDate, '2024-01-29', 'DOM leading cell matches C');
        setMarsDatePickerMonthYear(2025, 2);
        equal(marsDatePickerState.year, 2025, 'host year is a view of native state');
        equal(marsDatePickerState.month, 2, 'host month is a view of native state');
        equal(datetimeDate.value, '2024-02-29', 'uncommitted month selection preserves input');
        closeMarsDatePicker();
        equal(forms.lab_forms_picker_date(), 0, 'DOM close also closes the native controller');
        check(!labDefinitionScripts.includes('date_picker'), 'retired date picker script still loaded');
    } finally {
        integratorReferenceMetadata = savedReferences;
        requestLabForms = savedRequest;
        renderIntegratorRows(savedRows);
        datetimeDate.value = savedDate;
        integratorIntervalCap.value = savedIntervals;
        closeMarsDatePicker();
        Object.assign(
            marsDatePickerState, {input: savedPicker.input, button: savedPicker.button, shell: savedPicker.shell});
        if (!wasHidden) {
            forms.lab_forms_picker_open(savedPicker.year, savedPicker.month, savedPicker.year);
            savedPicker.shell?.classList.add('open');
            renderMarsDatePicker();
        }
    }
};
