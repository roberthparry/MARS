/**
 * @file event_checks.ts
 * @brief Native event-plan and live subscription regressions.
 *
 * Checks service ordering and argument identity while isolating browser effects and restoring replaced capabilities.
 */

/** Native event-plan and live subscription regressions; browser services are isolated and restored. */
window.checkLabEvents = function checkLabEvents() {
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error('Events: ' + message);
    };
    const native = labWire.exports();
    const plan = (action: number, event: object = {}) => labDOM.call<BindingTestPlan>('lab_events_dispatch', action, event);
    const names = (result: Pick<BindingTestPlan, 'calls'>) => result.calls.map(call => call.service).join(',');
    const expect = (action: number, event: object, services: string, args: unknown[] = []) => {
        const result = plan(action, event);
        check(names(result) === services, 'service order for action ' + action);
        if (services && !services.includes(',')) {
            check(result.calls[0].args.length === args.length, 'argument count for action ' + action);
            args.forEach((arg, index) => check(Object.is(result.calls[0].args[index], arg), 'argument identity'));
        }
        return result;
    };
    const services = [
        'clearForwardHistory',
        'clearGoalSeekRequest',
        'hideTargetEntry',
        'evaluateCurrentMode',
        'resetPlotPreview',
        'navigateHistoryFromEvent',
        'evaluateFromKeyboard',
        'selectWorksheetMode',
        'normaliseWorksheetControl',
        'startGoalSeekFromEvent',
        'setWorksheetPrecision',
        'toggleTextDigits',
        'copyResultFromEvent',
        'flushWorksheetState',
        'scheduleWorkspacePanelFit',
        'openMarsDatePicker',
        'closeMarsDatePicker',
        'applyCalendarControlEvent',
        'refreshWorksheetFromInput',
        'clearWorksheetFromEvent',
        'toggleHelp',
        'formatAlmanacTimeFromEvent',
        'toggleRenderedDigits',
        'runFunctionCard',
        'sendResultExpressionToInput',
        'copyInputFromEvent',
        'scheduleEditorResizeGrip'
    ];
    const calls: {service: string; args: unknown[]}[] = [], reentryErrors: unknown[] = [];
    const registry = Object.fromEntries(
        services.map(service => [service, (...args: unknown[]) => {
                         calls.push({service, args});
                         // Every host service must run after the native handle scope has been released.
                         try {
                             check(plan(255).calls.length === 0, 'service can re-enter the DOM bridge');
                         } catch (error) {
                             reentryErrors.push(error);
                         }
                     }]));
    const savedMode = native.lab_workspace_mode();
    const savedExpansion = native.lab_view_card_expanded();
    const savedZoom = WorksheetContext.resultCards.map(card => {
        const id = WorksheetContext.resultCardIds.get(card)!;
        return [id, native.lab_view_card_zoom(id)] as const;
    });
    const savedRequestFrame = window.requestAnimationFrame, savedCancelFrame = window.cancelAnimationFrame;
    const savedFrame = WorksheetWorkspace.layout.editorResizeFrame;
    const snapshots = [document.body, ...document.body.querySelectorAll('*')].map(
        node => ({
            node,
            attributes:
                [...node.attributes].map(attribute => [attribute.namespaceURI, attribute.name, attribute.value] as const),
            children: [...node.childNodes],
            value: (node instanceof HTMLInputElement || node instanceof HTMLSelectElement || node instanceof HTMLTextAreaElement) ? node.value : undefined,
            selected: node instanceof HTMLOptionElement ? node.selected : undefined
        }));
    const fixture = document.createElement('div');
    fixture.innerHTML = '<textarea></textarea><button class="mode-tab" data-mode="matrix"><span>tab</span></button>';
    // Exercise nested tab bubbling without invoking unrelated document picker/tooltip handlers.
    fixture.addEventListener('click', event => event.stopPropagation());
    const send = (target: EventTarget | null, event: Event, expected: string) => {
        check(!!target, 'registered target exists');
        calls.length = 0;
        if (!target) throw new Error('Registered target missing');
        const accepted = target.dispatchEvent(event);
        check(names({calls}) === expected, 'live ' + event.type + ' routing: ' + names({calls}));
        return accepted;
    };
    const click = () => new MouseEvent('click', {cancelable: true});
    const key = (options: KeyboardEventInit) => new KeyboardEvent('keydown', {cancelable: true, ...options});
    const savedRegistry = labDOM.bindEvents(Object.freeze(registry));
    try {
        window.requestAnimationFrame = () => 0;
        window.cancelAnimationFrame = () => {};
        const refreshEditor = document.createElement('textarea');
        refreshEditor.value = '  opaque μ {x = ?}  ';
        refreshEditor.dataset.bindingRefreshValid = 'pending';
        refreshEditor.dataset.evaluationReady = 'keep';
        const refreshPlan = (mode: number, bound: boolean, unchanged: boolean) => labDOM.call<BindingTestPlan>(
            'lab_events_refresh', mode, Number(bound), Number(unchanged),
            {editor: refreshEditor, defaultDatetimeText: 'calendar default'});
        const expectedRefresh = [
            'saveState,clearGoalSeekRequest,setSource,scheduleEditedExpressionBindingRefresh',
            'refreshVariableValuesFromEditor,saveState,updateHistoryButtons',
            'setSource,setSource,updateHistoryButtons', 'refreshVariableValuesFromEditor,updateHistoryButtons',
            'refreshVariableValuesFromEditor,updateHistoryButtons', 'setModeEditor,updateHistoryButtons',
            'updateHistoryButtons'
        ];
        for (let mode = 0; mode < 7; ++mode) {
            for (const bound of [false, true]) {
                for (const unchanged of [false, true]) {
                    const refresh = refreshPlan(mode, bound, unchanged);
                    let expected = expectedRefresh[mode];
                    if (!mode && unchanged)
                        expected = 'saveState,cancelBindingRefresh,markBindingRefresh,updateHistoryButtons';
                    if (!bound && [1, 3, 4].includes(mode))
                        expected = 'setSource,setSource,' + expected;
                    check(names(refresh) === expected, 'native refresh service order for mode ' + mode);
                    const save = refresh.calls.find(call => call.service === 'saveState');
                    if (save) {
                        check(save.args[0] === WorksheetWorkspace.WORKSPACE_MODE_NAMES[mode], 'save uses the exact mode token');
                        check(Reflect.get(Object(Reflect.get(Object(save.args[1]), 'options')), 'debounce') === true, 'native save policy requests debounce');
                        check(
                            mode ? !Object.hasOwn(Object(save.args[1]), 'text') : Reflect.get(Object(save.args[1]), 'text') === 'opaque μ {x = ?}',
                            'expression saves trimmed text; equation preserves its live default');
                    }
                    const source = refresh.calls.filter(call => call.service === 'setSource');
                    if (source.length === 2)
                        check(
                            source[0].args[0] === 'fullText' && source[1].args[0] === 'displayText' &&
                                source.every(call => call.args[1] === 'opaque μ {x = ?}'),
                            'native source writes remain ordered and opaque');
                    check(
                        refreshEditor.dataset.bindingRefreshValid === 'pending',
                        'plan construction cannot mark reuse before cancelling its timer');
                }
            }
        }
        check(names(refreshPlan(99, false, false)) === '', 'invalid refresh mode has no service effects');
        refreshEditor.value = ' \u2003 ';
        check(refreshPlan(5, false, false).calls[0].args[1] === 'calendar default', 'blank Datetime editor fallback');
        refreshEditor.value = ' authored calendar ';
        check(refreshPlan(5, false, false).calls[0].args[1] === 'authored calendar', 'authored calendar text survives');
        const reused = refreshPlan(0, false, true);
        refreshPlan(6, true, false);
        check(reused.calls[2].args[0] === refreshEditor, 'deferred projection retains a node, not a scoped handle');
        labDOM.call('lab_events_refresh_mark', refreshEditor);
        check(
            refreshEditor.dataset.bindingRefreshValid === 'true' && refreshEditor.dataset.evaluationReady === 'keep',
            'native reuse projection changes only binding validity');

        const refreshSaved = {
            cancel: WorksheetRequests.labRequests.cancel,
            clearTimeout: window.clearTimeout,
            bindingParts: WorksheetBindings.editor.bindingParts,
            refreshVariableValuesFromEditor: WorksheetBindings.controller.refreshVariableValuesFromEditor,
            saveWorksheetState: WorksheetPersistence.persistence.saveWorksheet,
            clearGoalSeekRequest: WorksheetBindings.goal.clearGoalSeekRequest,
            scheduleEditedExpressionBindingRefresh: WorksheetBindings.controller.scheduleEditedExpressionBindingRefresh,
            updateHistoryButtons: WorksheetWorkspace.presentation.updateHistoryButtons,
            source: Object.fromEntries(Object.keys(WorksheetContext.labEditorState).map(key => [key, Reflect.get(WorksheetContext.labEditorState, key)])),
            editors: Object.fromEntries(WorksheetWorkspace.WORKSPACE_MODE_NAMES.map(mode => [mode, WorksheetContext.modeEditorText[mode]]))
        };
        const refreshCalls: string[] = [];
        const sourceDescriptor = Object.getOwnPropertyDescriptor(WorksheetContext.labEditorState, 'fullText')!;
        const modeDescriptor = Object.getOwnPropertyDescriptor(WorksheetContext.modeEditorText, 'datetime')!;
        let refreshBound = false;
        try {
            WorksheetRequests.labRequests.cancel = operation => refreshCalls.push('cancel:' + operation);
            Reflect.set(WorksheetBindings.editor, 'bindingParts', () => {
                check(
                    refreshCalls.join(',') === 'cancel:evaluate,cancel:bindings',
                    'requests cancel before flag discovery');
                return refreshBound ? {} : null;
            });
            Reflect.set(WorksheetBindings.controller, 'refreshVariableValuesFromEditor', () => {
                refreshCalls.push('bindings');
                check(
                    WorksheetContext.labEditorState.fullText === (refreshBound ? 'previous full' : 'opaque input'),
                    'source setter precedes binding refresh without redefining its view');
            });
            Reflect.set(WorksheetPersistence.persistence, 'saveWorksheet', (mode: string, text: string | undefined, options: {debounce: boolean}) => {
                refreshCalls.push('save');
                check(
                    options.debounce === true && text === (mode === 'expression' ? 'opaque input' : undefined),
                    'host adapter preserves omitted save text and native debounce policy');
                check(WorksheetContext.expr.dataset.bindingRefreshValid === 'pending', 'save precedes reuse projection');
            });
            Reflect.set(WorksheetBindings.goal, 'clearGoalSeekRequest', () => {
                refreshCalls.push('goal');
                check(WorksheetContext.labEditorState.lastInput === 'previous input', 'goal reset precedes last-input reset');
            });
            Reflect.set(WorksheetBindings.controller, 'scheduleEditedExpressionBindingRefresh', () => {
                refreshCalls.push('schedule');
                check(WorksheetContext.labEditorState.lastInput === '', 'last input resets before the binding timer is scheduled');
            });
            window.clearTimeout = timer => {
                refreshCalls.push('timer');
                check(
                    timer === WorksheetBindings.controller.expressionBindingRefreshTimer && WorksheetContext.expr.dataset.bindingRefreshValid === 'pending',
                    'pending refresh timer is cancelled before native reuse projection');
            };
            Reflect.set(WorksheetWorkspace.presentation, 'updateHistoryButtons', () => {
                refreshCalls.push('history');
                check(
                    labDOM.call<BindingTestPlan>('lab_events_refresh', 99, 0, 0, {}).calls.length === 0,
                    'refresh services can re-enter a released native scope');
                if (WorksheetWorkspace.mode.currentMode() === 'expression')
                    check(WorksheetContext.expr.dataset.bindingRefreshValid === 'true', 'reuse is projected before history refresh');
            });
            const hostOrder = [
                'save,goal,schedule', 'bindings,save,history', 'history', 'bindings,history', 'bindings,history',
                'history', 'history'
            ];
            for (let mode = 0; mode < 7; ++mode) {
                for (const bound of [false, true]) {
                    for (const unchanged of mode === 0 ? [false, true] : [false]) {
                        native.lab_workspace_select(mode);
                        refreshBound = bound;
                        WorksheetContext.expr.value = '  opaque input  ';
                        WorksheetContext.expr.dataset.bindingRefreshValid = 'pending';
                        WorksheetContext.labEditorState.fullText = 'previous full';
                        WorksheetContext.labEditorState.displayText = unchanged ? 'opaque input' : 'previous display';
                        WorksheetContext.labEditorState.lastInput = 'previous input';
                        refreshCalls.length = 0;
                        WorksheetBindings.controller.refreshWorksheetFromInput();
                        const expected = unchanged ? 'save,timer,history' : hostOrder[mode];
                        check(
                            refreshCalls.join(',') === 'cancel:evaluate,cancel:bindings,' + expected,
                            'real refresh adapter preserves service order for mode ' + mode);
                        if (mode === 2 || (!bound && [1, 3, 4].includes(mode)))
                            check(
                                WorksheetContext.labEditorState.fullText === 'opaque input' &&
                                    WorksheetContext.labEditorState.displayText === 'opaque input',
                                'real source view setters receive native text');
                        if (mode === 5)
                            check(
                                WorksheetContext.modeEditorText.datetime === 'opaque input',
                                'real calendar view setter receives native text');
                    }
                }
            }
            check(
                Object.getOwnPropertyDescriptor(WorksheetContext.labEditorState, 'fullText')!.get === sourceDescriptor.get &&
                    Object.getOwnPropertyDescriptor(WorksheetContext.labEditorState, 'fullText')!.set === sourceDescriptor.set &&
                    Object.getOwnPropertyDescriptor(WorksheetContext.modeEditorText, 'datetime')!.get === modeDescriptor.get,
                'native plans leave source and mode view descriptors intact');
        } finally {
            WorksheetRequests.labRequests.cancel = refreshSaved.cancel;
            window.clearTimeout = refreshSaved.clearTimeout;
            for (const name of ['bindingParts', 'refreshVariableValuesFromEditor', 'saveWorksheetState',
                'clearGoalSeekRequest', 'scheduleEditedExpressionBindingRefresh', 'updateHistoryButtons'] as const)
                Reflect.set(labTestModuleOwner(name), name, refreshSaved[name]);
            for (const [key, text] of Object.entries(refreshSaved.source)) Reflect.set(WorksheetContext.labEditorState, key, text);
            for (const [mode, text] of Object.entries(refreshSaved.editors)) Reflect.set(WorksheetContext.modeEditorText, mode, text);
            native.lab_workspace_select(savedMode);
        }
        check(
            savedRegistry && services.every(service => typeof savedRegistry[service] === 'function'),
            'bootstrap supplies every native service');
        native.lab_workspace_select(0);
        const runServices = 'resetPlotPreview,clearForwardHistory,clearGoalSeekRequest,hideTargetEntry,evaluateCurrentMode';
        expect(0, {}, runServices);
        // Check bootstrap registration before explicitly installing anything in this suite.
        send(document.getElementById('run'), click(), runServices);
        for (let mode = 1; mode < 7; ++mode) {
            native.lab_workspace_select(mode);
            const run = mode <= 2 || mode === 4 ? 'resetPlotPreview,evaluateCurrentMode' : 'evaluateCurrentMode';
            expect(0, {}, run);
            const result = expect(3, {key: 'Enter', metaKey: true}, run);
            check(result.prevent && !result.stop, 'non-expression shortcut cancellation');
        }
        native.lab_workspace_select(0);
        for (const modifier of ['ctrlKey', 'metaKey']) {
            const result = expect(3, {key: 'Enter', [modifier]: true}, 'resetPlotPreview,evaluateFromKeyboard');
            check(result.prevent && !result.stop, 'expression shortcut cancellation');
            check(
                !send(WorksheetContext.expr, key({key: 'Enter', [modifier]: true}), 'resetPlotPreview,evaluateFromKeyboard'),
                'real editor shortcut prevents the default');
        }
        for (const event of [{key: 'Enter'}, {key: 'Escape', ctrlKey: true}, {key: 'x', metaKey: true}]) {
            check(!expect(3, event, '').prevent, 'unrelated key retains browser behaviour');
            check(send(WorksheetContext.expr, key(event), ''), 'unrelated real key is not cancelled');
        }
        expect(1, {}, 'navigateHistoryFromEvent', [0]);
        expect(2, {}, 'navigateHistoryFromEvent', [1]);
        for (let action = 9; action <= 11; ++action) expect(action, {}, 'normaliseWorksheetControl', [action - 9]);
        // Equation variable is intentionally not a DOM control; its native action remains valid.
        expect(10, {currentTarget: null}, 'normaliseWorksheetControl', [1]);
        expect(14, {}, '');
        expect(15, {}, '');
        const slider = WorksheetContext.precisionSlider;
        check(['value', 'min', 'max', 'step'].every(name => !Object.hasOwn(slider, name)),
              'precision slider must use native range properties rather than shadowing them');
        const originalPrecision = native.lab_workspace_requested_precision(native.lab_workspace_mode());
        const sliderPosition = originalPrecision === 384 ? 4 : 3;
        const sliderBits = native.lab_workspace_precision_slider_bits(sliderPosition);
        slider.disabled = false;
        slider.value = String(sliderPosition);
        const valueDescriptor = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!;
        let previewValueWrites = 0;
        Object.defineProperty(slider, 'value', {
            configurable: true,
            get() { return valueDescriptor.get!.call(this); },
            set(value: string) { ++previewValueWrites; valueDescriptor.set!.call(this, value); }
        });
        try {
            send(slider, new Event('input'), '');
            check(previewValueWrites === 0, 'preview must not reset the native range change event');
        } finally {
            Reflect.deleteProperty(slider, 'value');
        }
        check(native.lab_workspace_requested_precision(native.lab_workspace_mode()) === originalPrecision,
              'dragging previews without changing precision or starting evaluation');
        check(WorksheetContext.precisionValue.textContent === native.lab_workspace_digits(sliderBits) + ' digits' &&
              slider.getAttribute('aria-valuetext')?.includes(sliderBits + ' bits'),
              'native preview is visible and accessible: ' + JSON.stringify({
                  position: slider.value, bits: sliderBits, text: WorksheetContext.precisionValue.textContent,
                  aria: slider.getAttribute('aria-valuetext')}));
        slider.disabled = true;
        send(slider, new Event('change'), '');
        slider.disabled = false;
        const button = document.getElementById('parsedMore');
        for (const [action, target] of [[17, WorksheetContext.parsed], [18, WorksheetContext.functionStyle], [20, WorksheetContext.value]] as const)
            expect(action, {currentTarget: button}, 'toggleTextDigits', [target, button]);
        const retained = expect(23, {currentTarget: button}, 'copyResultFromEvent', [button]);
        plan(255, {currentTarget: WorksheetContext.expr});
        check(retained.calls[0].args[0] === button, 'returned arguments are nodes, not recycled handle numbers');
        check(expect(13, {key: 'Enter'}, 'startGoalSeekFromEvent').prevent, 'goal Enter is cancelled');
        check(!expect(13, {key: 'Tab'}, '').prevent, 'goal Tab is untouched');
        for (const action of [35, 255, 256, (4 << 8), (5 << 8) | 255, (6 << 8) | 255, (7 << 8)])
            check(!expect(action, {}, '').prevent, 'unknown action or calendar field is harmless');

        const routes: [string, string, number, string, unknown[]][] = [
            ['back', 'click', 1, 'navigateHistoryFromEvent', [0]],
            ['forward', 'click', 2, 'navigateHistoryFromEvent', [1]],
            ['clear', 'click', 5, 'clearWorksheetFromEvent', []], ['help', 'click', 6, 'toggleHelp', []],
            ['helpClose', 'click', 6, 'toggleHelp', []],
            ['goalSeek', 'click', 7, 'startGoalSeekFromEvent', []],
            ['matrixOperation', 'change', 9, 'normaliseWorksheetControl', [0]],
            ['integratorIntervalCap', 'change', 11, 'normaliseWorksheetControl', [2]],
            ['almanacTime', 'input', 12, 'formatAlmanacTimeFromEvent', []],
            ['precisionSlider', 'change', 14, 'setWorksheetPrecision', [sliderBits]],
            ['renderedMore', 'click', 16, 'toggleRenderedDigits', []],
            ['functionRun', 'click', 19, 'runFunctionCard', []],
            ['resultUseInput', 'click', 21, 'sendResultExpressionToInput', []],
            ['inputCopy', 'click', 22, 'copyInputFromEvent', []]
        ];
        for (const [id, type, action, service, args] of routes) {
            const target = document.getElementById(id);
            expect(action, {currentTarget: target}, service, args);
            send(target, new Event(type, {cancelable: true}), service);
            check(
                calls[0].args.length === args.length && args.every((arg, index) => calls[0].args[index] === arg),
                'live service arguments for ' + id);
        }
        {
            const rect = slider.getBoundingClientRect();
            check(rect.width > 16, 'precision track has measurable width');
            const press = (clientX: number) => new PointerEvent('pointerdown', {
                button: 0, clientX, clientY: rect.top + rect.height / 2, cancelable: true
            });
            // The isolated event registry does not include document tooltip services triggered by focus.
            const focus = slider.focus;
            slider.focus = () => {};
            try {
                slider.value = '0';
                check(!send(slider, press(rect.right - 1), 'setWorksheetPrecision') && calls[0].args[0] === 106,
                  'clicking the distant track from binary64 advances only to double-double');
                check(!send(slider, press(rect.right - 1), 'setWorksheetPrecision') && calls[0].args[0] === 192,
                  'the next track click selects 192 bits');
                check(!send(slider, press(rect.left), 'setWorksheetPrecision') && calls[0].args[0] === 106,
                  'left track click retreats one precision setting');
                slider.value = '0';
                check(send(slider, press(rect.left + 8), ''), 'pressing the thumb retains native dragging');
            } finally {
                slider.focus = focus;
                slider.value = String(sliderPosition);
            }
        }
        send(WorksheetContext.expr, new Event('input'), 'refreshWorksheetFromInput,scheduleEditorResizeGrip');
        for (const [id, target] of [['parsedMore', WorksheetContext.parsed], ['functionMore', WorksheetContext.functionStyle], ['valueMore', WorksheetContext.value]] as const) {
            const control = document.getElementById(id);
            send(control, click(), 'toggleTextDigits');
            check(calls[0].args[0] === target && calls[0].args[1] === control, 'real digit-button node identity');
        }
        const copyButton = document.querySelector('[data-copy-target]');
        send(copyButton, click(), 'copyResultFromEvent');
        check(calls[0].args[0] === copyButton, 'copy button identity survives native return');

        const calendarFlags: Record<string, Record<string, number>> = {
            datetime: {
                date: 19,
                jdn: 0,
                start: 1,
                end: 1,
                year: 0,
                jurisdiction: 48,
                town: 4,
                latitude: 8,
                longitude: 8,
                elevation: 8,
                gmt_offset: 0
            },
            almanac: {
                date: 17,
                time: 0,
                zone: 0,
                jurisdiction: 48,
                town: 4,
                latitude: 8,
                longitude: 8,
                elevation: 8,
                visibility: 0
            }
        };
        for (const [mode, flags] of Object.entries(calendarFlags)) {
            for (const field of calendarSchema(mode)) {
                const target = field.element ? document.getElementById(field.element) : null;
                check(Object.hasOwn(flags, field.key), 'calendar field has independent expected policy');
                check(typeof field.id === 'number' && typeof field.field === 'number', 'numeric calendar route');
                if (typeof field.id !== 'number' || typeof field.field !== 'number') throw new Error('Invalid calendar route');
                const action = (field.id << 8) | field.field;
                if (target)
                    expect(
                        action, {type: 'change', currentTarget: target}, 'applyCalendarControlEvent',
                        [mode, flags[field.key]]);
                if (!(flags[field.key] & 1)) {
                    expect(action, {type: 'keydown', key: 'Enter', currentTarget: target}, '');
                    continue;
                }
                const pickerButton = target!.closest('.mars-date-shell')!.querySelector('[data-date-target]');
                for (const name of ['ArrowDown', 'Enter']) {
                    const result = expect(
                        action, {type: 'keydown', key: name, currentTarget: target}, 'openMarsDatePicker',
                        [target, pickerButton]);
                    check(result.prevent && !result.stop, 'calendar open prevents default only');
                    check(!send(target, key({key: name}), 'openMarsDatePicker'), 'real date shortcut cancellation');
                    check(calls[0].args[0] === target && calls[0].args[1] === pickerButton, 'real picker arguments');
                }
                check(
                    !expect(action, {type: 'keydown', key: 'Escape', currentTarget: target}, 'closeMarsDatePicker')
                         .prevent,
                    'calendar Escape preserves default');
                expect(action, {type: 'keydown', key: 'Enter', currentTarget: document.createElement('input')}, '');
            }
        }
        const dateField = calendarSchema('datetime').find(field => field.key === 'date')!;
        const dateInput = document.querySelector<HTMLInputElement>('#' + dateField.element)!;
        dateInput.value = '2031-02-03';
        send(dateInput, new Event('change'), 'applyCalendarControlEvent');
        check(WorksheetContext.datetimeYear.value === '2031' && WorksheetContext.datetimeJdn.value === '', 'date change projects year and clears JDN');
        dateInput.value = '';
        WorksheetContext.datetimeYear.value = '1984';
        WorksheetContext.datetimeJdn.value = '2451545';
        send(dateInput, new Event('change'), 'applyCalendarControlEvent');
        check(WorksheetContext.datetimeYear.value === '1984' && WorksheetContext.datetimeJdn.value === '', 'empty date preserves year but clears JDN');

        // Observe actual listener options, including new targets and repeated installation.
        document.body.append(fixture);
        const testDocument: EventTarget & {visibilityState?: DocumentVisibilityState} = new EventTarget();
        const testWindow = new EventTarget();
        const subscriptions: {target: EventTarget; type: string; options?: boolean | AddEventListenerOptions}[] = [];
        const add = EventTarget.prototype.addEventListener;
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            subscriptions.push({target: this, type, options});
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_events_install', testDocument, testWindow);
            const count = subscriptions.length;
            check(count > 0, 'new targets receive subscriptions');
            labDOM.call('lab_events_install', testDocument, testWindow);
            check(subscriptions.length === count, 'installation deduplicates target/type/action');
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        const scroll = subscriptions.find(entry => entry.target === testWindow && entry.type === 'scroll');
        check(typeof scroll?.options === 'object' && scroll.options.passive === true, 'scroll listener is passive');
        check(
            subscriptions.filter(entry => entry !== scroll).every(entry => typeof entry.options === 'object' && entry.options.passive === false),
            'cancellable listeners explicitly remain non-passive');
        send(fixture.querySelector('textarea'), new Event('input'), 'scheduleEditorResizeGrip');
        send(fixture.querySelector('span'), new MouseEvent('click', {bubbles: true}), 'selectWorksheetMode');
        check(calls[0].args[0] === 'matrix', 'tab uses currentTarget rather than nested clicked element');
        send(testWindow, new Event('pagehide'), 'flushWorksheetState');
        send(testWindow, new Event('resize'), 'scheduleWorkspacePanelFit');
        send(testWindow, new Event('scroll'), 'scheduleWorkspacePanelFit');
        testDocument.visibilityState = 'visible';
        send(testDocument, new Event('visibilitychange'), '');
        testDocument.visibilityState = 'hidden';
        send(testDocument, new Event('visibilitychange'), 'flushWorksheetState');
        send(document.getElementById('run'), click(), runServices);

        const card = WorksheetContext.resultCards.find(
            candidate => candidate.querySelector('[data-expand-card]') && candidate.querySelector('[data-zoom-step]') &&
                candidate.querySelector('[data-zoom-reset]'))!;
        check(!!card, 'a registered card has expansion and zoom controls');
        const cardId = WorksheetContext.resultCardIds.get(card)!, initialZoom = native.lab_view_zoom_index(NaN);
        native.lab_view_card_set_zoom(cardId, initialZoom);
        check(send(card, new WheelEvent('wheel', {deltaY: -1, cancelable: true}), ''), 'plain wheel is not captured');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'plain wheel does not zoom');
        check(
            !send(card, new WheelEvent('wheel', {deltaY: -1, ctrlKey: true, cancelable: true}), ''),
            'modified wheel is cancellable despite listener options');
        check(native.lab_view_card_zoom(cardId) === initialZoom + 1, 'modified wheel zooms upwards');
        check(
            !send(card, new WheelEvent('wheel', {deltaY: 1, metaKey: true, cancelable: true}), ''),
            'meta wheel is cancelled');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'positive wheel zooms downwards');
        const zoom = card.querySelector('[data-zoom-step]'), reset = card.querySelector('[data-zoom-reset]');
        check(zoom && reset, 'zoom controls are present');
        let bubbled = false;
        const observe = () => {
            bubbled = true;
        };
        card.addEventListener('click', observe);
        try {
            check(!send(zoom, new MouseEvent('click', {bubbles: true, cancelable: true}), ''), 'zoom cancels click');
            check(!bubbled, 'zoom prevents ancestor click propagation');
        } finally {
            card.removeEventListener('click', observe);
        }
        send(reset, click(), '');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'reset restores native default zoom');
        native.lab_view_cards_collapse();
        const expand = card.querySelector('[data-expand-card]')!;
        send(expand, click(), '');
        check(
            native.lab_view_card_expanded() === cardId && expand.getAttribute('aria-expanded') === 'true',
            'expand subscription projects native state');
        let addedWhileExpanded = 0;
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            ++addedWhileExpanded;
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_events_install', testDocument, testWindow);
            labDOM.call('lab_events_install', testDocument, testWindow);
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        check(addedWhileExpanded === 0, 'expanded-card reinstallation adds no duplicate listeners');
        check(
            native.lab_view_card_expanded() === cardId && expand.getAttribute('aria-expanded') === 'true' &&
                card.classList.contains('expanded-card') && WorksheetContext.labWorkspace.classList.contains('result-card-expanded') &&
                WorksheetContext.resultPane.classList.contains('card-expanded') && expand.textContent === 'Collapse',
            'reinstallation preserves native expansion and projects its accessibility and layout state');
        send(document.getElementById('run'), click(), runServices);
        send(testWindow, new Event('pagehide'), 'flushWorksheetState');
        send(expand, click(), '');
        check(
            native.lab_view_card_expanded() === -1 && expand.getAttribute('aria-expanded') === 'false',
            'second expand click collapses once');
        check(reentryErrors.length === 0, 'host services run outside the synchronous WASM scope: ' + reentryErrors);
    } finally {
        fixture.remove();
        native.lab_workspace_select(savedMode);
        for (const [id, zoom] of savedZoom) native.lab_view_card_set_zoom(id, zoom);
        native.lab_view_cards_collapse();
        if (savedExpansion >= 0)
            native.lab_view_card_toggle(savedExpansion);
        for (const {node, attributes, children} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [namespace, name, text] of attributes) node.setAttributeNS(namespace, name, text);
            node.replaceChildren(...children);
        }
        for (const {node, value: savedValue, selected} of snapshots) {
            if (savedValue !== undefined)
                Reflect.set(node, 'value', savedValue);
            if (selected !== undefined)
                Reflect.set(node, 'selected', selected);
        }
        WorksheetWorkspace.layout.editorResizeFrame = savedFrame;
        WorksheetResults.renderedFit.dispose();
        WorksheetResults.solverFit.dispose();
        window.requestAnimationFrame = savedRequestFrame;
        window.cancelAnimationFrame = savedCancelFrame;
        labDOM.bindEvents(savedRegistry);
    }
    return true;
};
