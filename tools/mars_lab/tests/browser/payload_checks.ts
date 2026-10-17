/**
 * @file payload_checks.ts
 * @brief Native request payload and guarded background-result regressions.
 *
 * Checks exact wire fields through the real codec and scoped bridge, including background application and ownership guards.
 */

/** Payload construction and guarded background application through the real scoped C/WASM bridge. */
window.checkLabPayload = async function checkLabPayload() {
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error('Native payload: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) => check(Object.is(actual, expected), message);
    const decodeRecord = (buffer: MarsTransportBuffer): Record<string, unknown> => {
        const value = labWire.decode(buffer);
        const isRecord = (item: unknown): item is Record<string, unknown> =>
            typeof item === 'object' && item !== null && !Array.isArray(item);
        if (!isRecord(value)) throw new Error('Expected a decoded payload record');
        return value;
    };
    const build = (kind: number, inputs: unknown[], request: {channel: number; token: number; plotKind?: string} | null = null) =>
        labDOM.call<Record<string, unknown>>('lab_payload_build', kind, inputs, request, 256, 1700000000123);
    const shape = (record: object, keys: string[]) => equal(Object.keys(record).sort().join(), keys.sort().join(), 'exact wire fields');
    const controls =
        [WorksheetContext.expr, WorksheetContext.matrixOperation, WorksheetContext.matrixOperand, WorksheetContext.almanacJurisdiction, WorksheetContext.almanacZone, WorksheetContext.almanacLatitude, WorksheetContext.almanacLongitude];
    const savedValues = controls.map(node => node?.value);
    const mobileNodes = [WorksheetContext.mobileAccess, WorksheetContext.mobileTitle, WorksheetContext.mobileHint, WorksheetContext.mobileUrl, WorksheetContext.mobileQr].filter(Boolean);
    const mobileSaved =
        mobileNodes.map(node => ({node, children: Array.from(node.childNodes), className: node.className}));
    const native = labWire.exports();
    try {
        equal(build(99, []), null, 'unknown builder rejected');
        equal(build(-1, []), null, 'negative builder rejected');
        const text = '  {sin([α]) | [α] = π/7, const c = ?}  ';
        const expression = build(0, [text, text, 'π/7', '[α]', 'bindings', 'expression']);
        shape(expression, [
            'expression', 'binding_source', 'binding_value', 'wrt', 'precision', 'action', 'expression_updated_at',
            'persist_expression'
        ]);
        equal(expression.expression, text, 'authored expression preserved without parsing or substitution');
        equal(expression.binding_source, text, 'binding source preserved exactly');
        equal(expression.binding_value, 'π/7', 'exact symbolic binding value retained');
        equal(expression.wrt, '[α]', 'authored calculus variable retained');
        equal(expression.action, 'bindings', 'legacy action retained');
        equal(expression.precision, 256, 'raw numerical precision boxed as a wire number');
        equal(expression.expression_updated_at, 1700000000123, 'timestamp retains double precision');
        equal(expression.persist_expression, true, 'legacy expression mode persists');
        equal(build(0, [0, false, '', '', '', 'matrix']).expression, '', 'falsy source conversion');
        equal(build(0, [17, '', '', '', '', 'matrix']).expression, '17', 'numeric sources are boxed in inputs');
        equal(build(0, ['', '', '', '', '', 'matrix']).persist_expression, false, 'legacy matrix does not persist');
        const roundTrip = decodeRecord(labWire.encode(expression));
        equal(roundTrip.persist_expression, true, 'persistence retains Boolean wire type');
        equal(roundTrip.expression_updated_at, expression.expression_updated_at, 'wire timestamp unchanged');
        const plot = build(0, ['sin(x)', '', '', 'x', 'plot', 'expression']);
        equal(plot.plot_source_mode, 'expression', 'plot source mode remains explicit');
        for (const mode of ['equation', 'diffequation']) {
            const solver = build(0, ['opaque mathematical source', '', '', 'x', 'plot', mode]);
            equal(solver.plot_source_mode, mode, 'solver plot mode reaches the native parser');
            equal(decodeRecord(labWire.encode(solver)).plot_source_mode, mode, 'solver mode survives Protobuf');
        }
        equal(plot.plot_lower, document.querySelector<HTMLInputElement>('#' + 'plotLower')!.value, 'plot lower bound remains opaque text');
        equal(plot.plot_upper, document.querySelector<HTMLInputElement>('#' + 'plotUpper')!.value, 'plot upper bound remains opaque text');
        for (const [key, id] of [['plot_kind', 'plotKind'], ['plot_second', 'plotSecond'],
                                ['plot_second_lower', 'plotSecondLower'], ['plot_second_upper', 'plotSecondUpper'],
                                ['plot_azimuth', 'plotAzimuth'], ['plot_elevation', 'plotElevation'],
                                ['plot_roll', 'plotRoll']]) {
            equal(plot[key], document.querySelector<HTMLInputElement>('#' + id)!.value, 'surface control remains opaque');
            equal(decodeRecord(labWire.encode(plot))[key], plot[key], 'surface control survives Protobuf');
        }
        equal(plot.plot_scale, document.querySelector<HTMLInputElement>('#' + 'plotScale')!.value, 'plot scale policy retained');
        equal(plot.plot_quality, 'full', 'ordinary plots always request full detail');
        const preview = build(0, ['x+y', '', '', 'x', 'plot', 'expression', true]);
        equal(preview.plot_quality, 'preview', 'native gesture selects reduced preview detail');
        equal(decodeRecord(labWire.encode(preview)).plot_quality, 'preview', 'preview quality survives Protobuf');
        equal(plot.plot_y_lower, document.querySelector<HTMLInputElement>('#' + 'plotYLower')!.value, 'manual Y minimum remains opaque');
        equal(plot.plot_y_upper, document.querySelector<HTMLInputElement>('#' + 'plotYUpper')!.value, 'manual Y maximum remains opaque');
        equal(decodeRecord(labWire.encode(plot)).action, 'plot', 'plot request uses Protobuf');

        // A targeted space-curve redraw must retain its dimensionality even at a real-valued sample range.
        native.lab_request_set_mode(0);
        const curveToken = native.lab_request_begin(0, 14, 1, 1, 0, 0) >>> 0;
        const curveRequest = {channel: native.lab_request_channel(14), token: curveToken};
        check(curveToken, 'targeted curve plot request started');
        const kindControl = document.getElementById('plotKind')! as HTMLInputElement;
        const savedKind = kindControl.value;
        try {
            const targeted = build(0, ['x', '', '', 'x', 'plot', 'expression'],
                                   {...curveRequest, plotKind: 'complex_curve'});
            equal(targeted.plot_kind, 'complex_curve', 'targeted 3-D curve kind overrides ordinary plot selection');
            equal(decodeRecord(labWire.encode(targeted)).plot_kind, 'complex_curve',
                  'targeted curve kind survives Protobuf for real-valued samples');
            equal(kindControl.value, savedKind, 'internal curve kind does not change the visible plot selector');
            const ordinary = build(0, ['x', '', '', 'x', 'plot', 'expression'], curveRequest);
            equal(ordinary.plot_kind, savedKind, 'ordinary redraw does not inherit a previous targeted curve kind');
            const unrecognised = build(0, ['x', '', '', 'x', 'plot', 'expression'],
                                      {...curveRequest, plotKind: 'unrecognised'});
            equal(unrecognised.plot_kind, savedKind, 'unrecognised internal plot kinds cannot override the selector');
        } finally {
            native.lab_request_finish(curveRequest.channel, curveToken);
        }

        WorksheetContext.expr.value = '  [x, y]  ';
        WorksheetContext.matrixOperation.value = 'eval';
        WorksheetContext.matrixOperand.value = '  [a + b, a - b]  ';
        const matrix = build(1, [{}, '', false]);
        shape(matrix, ['matrix', 'operation', 'operand', 'precision', 'transient']);
        equal(matrix.matrix, '[x, y]', 'matrix source falls back to trimmed editor');
        equal(matrix.operand, '[a + b, a - b]', 'undefined operand uses trimmed control');
        equal(matrix.operation, 'eval', 'empty operation uses control');
        equal(matrix.transient, false, 'ordinary matrix evaluation persists');
        equal(build(1, [{}, '  exact matrix  ', false]).matrix, 'exact matrix', 'full editor source precedence');
        const explicit =
            build(1, [{matrixText: '  [π/7]  ', operation: 'det', operand: null, skipSave: true}, '', true]);
        equal(explicit.matrix, '[π/7]', 'explicit matrix source precedence');
        equal(explicit.operation, 'det', 'explicit operation retained');
        equal(explicit.operand, '', 'explicit null operand clears control fallback');
        equal(explicit.transient, true, 'legacy skip-save flag');
        equal(build(1, [{operand: 0}, '', true]).operand, '', 'explicit falsy operand preserved as empty');
        equal(build(1, [{operand: '  0  '}, '', true]).operand, '0', 'symbolic zero operand preserved');

        // Native request ownership overrides fallback mode, action and skip-save options.
        for (const [mode, operation, scalar, action, persist, transient] of [
                 [0, 0, 0, '', true, false], [0, 2, 0, 'integral', true, false], [0, 6, 0, 'bindings', true, false],
                 [3, 2, 0, 'integral', false, true], [3, 2, 1, 'integral', false, true], [3, 0, 0, '', false, false]] as const) {
            native.lab_request_set_mode(mode);
            const token = native.lab_request_begin(mode, operation, 1, 1, scalar, 0) >>> 0;
            const request = {channel: native.lab_request_channel(operation), token};
            check(token, 'fixture request started');
            try {
                const owned = build(0, ['x', '', '', 'x', 'wrong', 'wrong'], request);
                equal(owned.action, action, 'native request selects expression action');
                equal(owned.persist_expression, persist, 'native request selects persistence');
                const ownedMatrix = build(1, [{skipSave: !transient}, 'x', false], request);
                equal(ownedMatrix.transient, transient, 'native flags override skip-save option');
            } finally {
                native.lab_request_finish(request.channel, token);
            }
        }
        for (const [kind, field] of [[2, 'equation'], [3, 'diffequation']] as const) {
            const payload = build(kind, [text]);
            shape(payload, [field, 'precision']);
            equal(payload[field], text, 'equation full source is opaque and untrimmed');
            equal(build(kind, [''])[field], '[x, y]', 'empty equation source uses trimmed editor');
        }
        const bounds = [{name: '[α]', lo: 'a + b', hi: 'a - b', native_metadata: {opaque: true}}];
        const integral = build(4, [text, bounds, 20000]);
        shape(integral, ['expression', 'bounds', 'precision', 'max_intervals']);
        equal(integral.bounds, bounds, 'bound records and metadata retained by reference');
        equal(integral.expression, text, 'integrator source unchanged');
        equal(integral.max_intervals, 20000, 'boxed numerical interval cap');
        equal(labDOM.call('lab_payload_bounds_error', bounds), null, 'paired symbolic bounds accepted');
        for (const row of [{lo: '', hi: ''}, {lo: '', hi: '∞'}, {lo: '0', hi: '∞'}])
            equal(labDOM.call('lab_payload_bounds_error', [{name: 'x', ...row}]), null, 'supported bound shape');
        equal(
            labDOM.call('lab_payload_bounds_error', [{name: '[α]', lo: '0', hi: ''}]),
            'A one-sided bound for [α] should be entered as an upper bound. Leave lower blank and put the value in upper.',
            'lower-only bound diagnostic preserves authored name');
        equal(build(5, [text]).source, text, 'Function programme bytes retained');
        shape(build(5, [text]), ['source', 'precision']);
        const weather = build(6, [{date: '2026-10-10', latitude: '0', longitude: '-0', extra: 'not sent'}]);
        shape(weather, ['date', 'latitude', 'longitude']);
        equal(weather.latitude, '0', 'coordinate string is not numerically reinterpreted');
        equal(weather.longitude, '-0', 'signed coordinate text retained');

        const cell = (jd: string) => {
            const node = document.createElement('span');
            node.dataset.almanacLandTotality = jd;
            return node;
        };
        const cells = [' 2450000.5 ', '__proto__', 'constructor', 'missing', 'empty'].map(cell);
        const fields = {
            event_year: '2027',
            jurisdiction: 'native-jurisdiction',
            zone: 'native-zone',
            latitude: ' 0 ',
            longitude: ' -0 '
        };
        const land = labDOM.call<Record<string, unknown> & {events: {jd: string}[]}>('lab_payload_land', {event_year: '2028', fields}, cells, labApplication.configuration);
        shape(land, ['event_year', 'jurisdiction', 'zone', 'latitude', 'longitude', 'events']);
        equal(land.event_year, '2028', 'top-level event year precedence');
        equal(land.latitude, fields.latitude, 'native request field whitespace preserved');
        equal(land.longitude, fields.longitude, 'native coordinate source remains opaque');
        equal(land.jurisdiction, fields.jurisdiction, 'native jurisdiction precedence');
        equal(land.zone, fields.zone, 'native zone precedence');
        equal(land.events[0].jd, '2450000.5', 'event identifier boundary whitespace trimmed');
        equal(labDOM.call<Record<string, unknown> & {events: {jd: string}[]}>('lab_payload_land', {fields}, [], labApplication.configuration).event_year, '2027', 'nested year fallback');
        for (const node of [WorksheetContext.almanacJurisdiction, WorksheetContext.almanacZone, WorksheetContext.almanacLatitude, WorksheetContext.almanacLongitude]) {
            if (node)
                node.value = '';
        }
        const defaults = labDOM.call<Record<string, unknown> & {events: {jd: string}[]}>('lab_payload_land', {}, [], labApplication.configuration);
        equal(defaults.event_year, '', 'absent event year');
        equal(defaults.jurisdiction, WorksheetContext.DEFAULT_DATETIME_JURISDICTION, 'bootstrap jurisdiction fallback');
        equal(defaults.zone, WorksheetContext.DEFAULT_ALMANAC_ZONE, 'bootstrap zone fallback');
        equal(defaults.latitude, WorksheetContext.DEFAULT_ALMANAC_LATITUDE, 'bootstrap latitude fallback');
        equal(defaults.longitude, WorksheetContext.DEFAULT_ALMANAC_LONGITUDE, 'bootstrap longitude fallback');
        WorksheetContext.almanacLatitude.value = ' 51.5 ';
        equal(
            labDOM.call<Record<string, unknown> & {events: {jd: string}[]}>('lab_payload_land', {}, [], labApplication.configuration).latitude, WorksheetContext.almanacLatitude.value,
            'control fallback is passed through verbatim');

        const items = [
            {jd: '2450000.5', html: 'earlier'}, {jd: ' 2450000.5 ', html: '<b>native</b>'},
            {jd: '__proto__', html: '<i>prototype key</i>'}, {jd: 'constructor', html: 17}, {jd: 'empty', html: ''}
        ];
        labDOM.call('lab_payload_land_apply', cells, {items}, 0);
        equal(cells[0].innerHTML, '<b>native</b>', 'last duplicate wins and native markup is unchanged');
        equal(cells[1].innerHTML, '<i>prototype key</i>', 'prototype-named event is an ordinary Map key');
        equal(cells[2].textContent, 'No land totality found', 'non-string HTML uses fallback');
        equal(cells[3].textContent, 'No land totality found', 'unmatched event uses fallback');
        equal(cells[4].innerHTML, '', 'empty native HTML is a valid result');
        labDOM.call('lab_payload_land_apply', cells, {items: [], timed_out: true}, 0);
        equal(cells[0].textContent, 'Nearest land totality search timed out', 'native timeout fallback');
        for (const [failure, expected] of [
                 [1, 'Nearest land totality search timed out'], [2, 'Nearest land totality unavailable']]) {
            labDOM.call('lab_payload_land_apply', cells, {items}, failure);
            equal(cells[0].textContent, expected, 'failed request cannot install result markup');
        }
        const many = Array.from({length: 1200}, (_, index) => cell(String(index)));
        const manyPayload = labDOM.call<Record<string, unknown> & {events: {jd: string}[]}>('lab_payload_land', {}, many, labApplication.configuration);
        equal(manyPayload.events.length, many.length, 'payload handle scopes support long event lists');
        labDOM.call(
            'lab_payload_land_apply', many,
            {items: many.map((node, index) => ({jd: String(index), html: `<b>${index}</b>`}))}, 0);
        equal(many[1199].innerHTML, '<b>1199</b>', 'reply handle scopes support long event lists');

        if (WorksheetContext.mobileAccess && WorksheetContext.mobileUrl && WorksheetContext.mobileQr) {
            labDOM.call('lab_payload_mobile', {
                url: 'https://example.invalid/',
                title: '<b>Title</b>',
                hint: 'Native',
                qr: '<svg data-native-qr="1"></svg>'
            });
            equal(WorksheetContext.mobileUrl.textContent, 'https://example.invalid/', 'native URL retained');
            equal(WorksheetContext.mobileTitle?.textContent, WorksheetContext.mobileTitle ? '<b>Title</b>' : undefined, 'title remains plain text');
            check(WorksheetContext.mobileQr.querySelector('[data-native-qr]'), 'native QR markup installed');
            check(!WorksheetContext.mobileAccess.classList.contains('hidden'), 'access panel revealed');
            labDOM.call('lab_payload_mobile', {});
            equal(WorksheetContext.mobileUrl.textContent, 'Unavailable', 'absent URL fallback');
            equal(WorksheetContext.mobileQr.innerHTML, '', 'absent QR cleared');
        }
    } finally {
        controls.forEach((node, index) => {
            if (node)
                node.value = savedValues[index]!;
        });
        // Restore descendants before their parent so original node identities survive.
        mobileSaved.reverse().forEach(({node, children, className}) => {
            node.replaceChildren(...children);
            node.className = className;
        });
        WorksheetRequests.labRequests.modeChanged(WorksheetWorkspace.mode.currentMode());
    }

    const savedRequests = {
        post: WorksheetRequests.labRequests.post,
        begin: WorksheetRequests.labRequests.begin,
        current: WorksheetRequests.labRequests.current,
        finish: WorksheetRequests.labRequests.finish,
        context: WorksheetRequests.labRequests.context,
        cancel: WorksheetRequests.labRequests.cancel
    };
    const savedSave = WorksheetPersistence.persistence.saveWorksheet;
    const savedRefresh = WorksheetWorkspace.integrator.refreshIntegratorForms;
    const savedChildren = Array.from(WorksheetContext.rendered.childNodes);
    try {
        const sent: {request: unknown; payload: Record<string, unknown>; endpoint: string; signal?: AbortSignal}[] = [], savedModes: string[] = [];
        const nativeResult = {ok: true, presentation: {unknown_native_metadata: ['opaque']}};
        Reflect.set(WorksheetRequests.labRequests, 'post', async (request: unknown, payload: Record<string, unknown>, endpoint: string, signal?: AbortSignal) => {
            sent.push({request, payload, endpoint, signal});
            return {response: {ok: true}, data: nativeResult};
        });
        Reflect.set(WorksheetPersistence.persistence, 'saveWorksheet', (mode: string) => savedModes.push(mode));
        equal((await WorksheetRequests.evaluationTransport.fetchEvaluation('  x  ')).data, nativeResult, 'response metadata passed through unchanged');
        equal(sent.at(-1)!.payload.expression, '  x  ', 'existing expression entry point uses native builder');
        await WorksheetRequests.evaluationTransport.fetchMatrixEvaluation({matrixText: '[x]', skipSave: true});
        equal(savedModes.length, 0, 'transient matrix request does not save');
        await WorksheetRequests.evaluationTransport.fetchMatrixEvaluation({matrixText: '[x]'});
        equal(savedModes.join(), 'matrix', 'ordinary matrix request saves once');
        await WorksheetRequests.evaluationTransport.fetchEquationEvaluation();
        equal(sent.at(-1)!.endpoint, '/equation-eval', 'equation entry point retained');
        await WorksheetRequests.evaluationTransport.fetchDiffequationEvaluation();
        equal(sent.at(-1)!.endpoint, '/diffequation-eval', 'differential entry point retained');
        const controller = new AbortController();
        await WorksheetRequests.evaluationTransport.fetchDatetimeWeather({date: '2026-10-10', latitude: '0', longitude: '0'}, controller.signal);
        equal(sent.at(-1)!.signal, controller.signal, 'weather retains host cancellation signal');

        let context = 1, cancelled = '';
        let releaseRows!: (value: unknown) => void;
        const request = {} as WorksheetRequests.MarsApiRequest; // Ownership-only mock; no resource fields are consumed.
        WorksheetRequests.labRequests.context = () => context;
        WorksheetRequests.labRequests.current = () => true;
        WorksheetRequests.labRequests.cancel = operation => cancelled = operation;
        Reflect.set(WorksheetWorkspace.integrator, 'refreshIntegratorForms', () => new Promise<unknown>(resolve => {
            releaseRows = resolve;
        }));
        const before = sent.length;
        const preparation = WorksheetRequests.evaluationTransport.fetchIntegratorEvaluation(request).then(() => false, error => error.name === 'AbortError');
        ++context;
        releaseRows([]);
        check(await preparation, 'integrator rechecks context after awaited form preparation');
        equal(sent.length, before, 'stale form preparation cannot post a payload');
        equal(cancelled, 'evaluate', 'stale preparation cancels its still-current request');

        const target = document.createElement('span');
        target.dataset.almanacLandTotality = '2450000.5';
        target.textContent = 'retained';
        WorksheetContext.rendered.replaceChildren(target);
        let current = true, finished = 0;
        let releaseLand!: (value: unknown) => void;
        WorksheetRequests.labRequests.begin = () => request;
        WorksheetRequests.labRequests.current = () => current;
        WorksheetRequests.labRequests.finish = owner => {
            equal(owner, request, 'background request ownership retained');
            ++finished;
        };
        Reflect.set(WorksheetRequests.labRequests, 'post', () => new Promise<unknown>(resolve => {
            releaseLand = resolve;
        }));
        const pending = WorksheetRequests.calendarRequests.refreshAlmanacLandTotality({event_year: '2026'});
        current = false;
        releaseLand({response: {ok: true}, data: {ok: true, items: [{jd: '2450000.5', html: '<b>obsolete</b>'}]}});
        await pending;
        equal(target.textContent, 'retained', 'stale background result cannot reach C application');
        equal(finished, 1, 'stale background request still releases resources');
    } finally {
        Object.assign(WorksheetRequests.labRequests, savedRequests);
        Reflect.set(WorksheetPersistence.persistence, 'saveWorksheet', savedSave);
        Reflect.set(WorksheetWorkspace.integrator, 'refreshIntegratorForms', savedRefresh);
        WorksheetContext.rendered.replaceChildren(...savedChildren);
    }
};
